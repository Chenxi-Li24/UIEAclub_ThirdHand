#!/usr/bin/env python3
"""
Claude Agent Bridge — PC-side WebSocket + Claude API → UDP robot control.

Receives recognized speech text from ESP32-P4 via WebSocket,
forwards to Claude API with robot-control tools, and sends
generated UDP commands back to the ESP32-P4 for execution.

Architecture:
    ESP32-P4 (mic→ASR→text)
        │  WebSocket ws://192.168.58.100:9000
        ▼
    claude_agent.py  ←── Claude API (tool use: servoJ, servoStart, etc.)
        │  UDP :20008
        ▼
    ESP32-P4 → Fairino robot arm

Usage:
    pip install anthropic websockets
    export ANTHROPIC_API_KEY="sk-ant-..."
    python claude_agent.py [--port 9000] [--robot-ip 192.168.58.100] [--robot-port 20008]
"""

import asyncio
import json
import logging
import socket
import sys
import uuid
from argparse import ArgumentParser
from typing import Optional

# ── Dependencies (lazy-imported with helpful messages) ──────────────────
try:
    import websockets
    from websockets.server import WebSocketServerProtocol
except ImportError:
    print("ERROR: pip install websockets")
    sys.exit(1)

try:
    from anthropic import AsyncAnthropic, APIError, RateLimitError
except ImportError:
    print("ERROR: pip install anthropic")
    sys.exit(1)

# ── Logging ─────────────────────────────────────────────────────────────
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [AGENT] %(levelname)s %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("claude_agent")

# ── Robot Control Tools ─────────────────────────────────────────────────
ROBOT_TOOLS = [
    {
        "name": "servoJ",
        "description": (
            "Move the Fairino robot arm to absolute joint angles (degrees). "
            "J1=base rotation, J2=shoulder, J3=elbow, J4=wrist pitch, "
            "J5=wrist roll, J6=end-effector. Use cmdT to control speed "
            "(default 2.0 seconds). Always call servoStart before and "
            "servoEnd after a sequence of movements."
        ),
        "input_schema": {
            "type": "object",
            "properties": {
                "j1": {"type": "number", "description": "Joint 1 angle (base rotation, degrees)"},
                "j2": {"type": "number", "description": "Joint 2 angle (shoulder, degrees)"},
                "j3": {"type": "number", "description": "Joint 3 angle (elbow, degrees)"},
                "j4": {"type": "number", "description": "Joint 4 angle (wrist pitch, degrees)"},
                "j5": {"type": "number", "description": "Joint 5 angle (wrist roll, degrees)"},
                "j6": {"type": "number", "description": "Joint 6 angle (end-effector, degrees)"},
                "cmdT": {
                    "type": "number",
                    "description": "Command period in seconds (default 2.0, lower = faster)",
                    "default": 2.0,
                },
            },
            "required": ["j1", "j2", "j3", "j4", "j5", "j6"],
        },
    },
    {
        "name": "servoStart",
        "description": "Enable servo motors — MUST call before any movement commands.",
        "input_schema": {"type": "object", "properties": {}},
    },
    {
        "name": "servoEnd",
        "description": "Disable servo motors — call after movement sequence is complete.",
        "input_schema": {"type": "object", "properties": {}},
    },
    {
        "name": "getStatus",
        "description": "Request current robot joint angles and state from the arm.",
        "input_schema": {"type": "object", "properties": {}},
    },
    {
        "name": "say",
        "description": (
            "Speak a response to the user via TTS. Use this to confirm actions, "
            "report results, or ask clarifying questions. Keep responses concise "
            "and natural — the user is talking to a robot arm."
        ),
        "input_schema": {
            "type": "object",
            "properties": {
                "text": {"type": "string", "description": "Text to speak to the user (Chinese or English)"},
            },
            "required": ["text"],
        },
    },
]

SYSTEM_PROMPT = """You are a voice-controlled Fairino FR5 robot arm assistant. The user speaks to you in natural language (Chinese or English) and you control the robot arm using the available tools.

## Robot Context
- Fairino FR5 6-axis industrial robot arm
- Joint limits (approximate, degrees):
  J1: ±180°, J2: -155°~+155°, J3: -80°~+180°, J4: ±180°, J5: ±100°, J6: ±360°
- Default speed: cmdT=2.0 (2-second movement)
- Known home position: J1=60.5, J2=-69.6, J3=-91.0, J4=-84.3, J5=100.5, J6=-8.9
- The robot is physically present — be careful with motions near people or obstacles

## Interaction Rules
1. ALWAYS call servoStart before any movement, and servoEnd after completing.
2. If the user's command is ambiguous, use `say` to ask for clarification rather than guessing.
3. If the command is unsafe or impossible, explain why with `say` and refuse.
4. For multi-step tasks (e.g., "pick up the box"), plan the sequence and execute step by step.
5. Use `say` briefly to confirm actions, warn before movement, and report completion.
6. If the user just says hello or asks a question, respond with `say` — don't move the arm.
7. Estimate joint angles from natural descriptions:
   - "straight up" → all joints near 0 (vertical)
   - "forward" / "reach forward" → J2 ~ -30, J3 ~ -60, J4 ~ -90
   - "to the left/right" → rotate J1
   - "go home" → known home position above
8. Default to moderate speeds (cmdT ≥ 1.5s) unless the user asks for faster.

Be helpful, safe, and conversational. The user can hear you through TTS, so speak naturally."""


# ── Agent Session ────────────────────────────────────────────────────────
class AgentSession:
    """Per-connection state: Claude client, message history, UDP socket."""

    def __init__(
        self,
        client: AsyncAnthropic,
        robot_ip: str,
        robot_port: int,
        session_id: str,
    ):
        self.client = client
        self.robot_ip = robot_ip
        self.robot_port = robot_port
        self.session_id = session_id
        self.udp_sock: Optional[socket.socket] = None
        self.messages: list[dict] = [{"role": "system", "content": SYSTEM_PROMPT}]
        self.tool_results_pending: list[dict] = []

    def _ensure_udp(self):
        if self.udp_sock is None:
            self.udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.udp_sock.settimeout(0.5)

    def send_udp(self, text: str) -> bool:
        """Send a command string to the ESP32-P4 via UDP."""
        self._ensure_udp()
        try:
            data = text.encode("utf-8")
            self.udp_sock.sendto(data, (self.robot_ip, self.robot_port))
            log.info("[%s] UDP → %s:%d  %s", self.session_id[:6], self.robot_ip, self.robot_port, text.strip())
            return True
        except OSError as e:
            log.error("[%s] UDP send failed: %s", self.session_id[:6], e)
            return False

    def tool_to_cmd(self, tool_name: str, tool_input: dict) -> str:
        """Convert a Claude tool call to a robot UDP command string."""
        if tool_name == "servoJ":
            j1 = tool_input.get("j1", 0)
            j2 = tool_input.get("j2", 0)
            j3 = tool_input.get("j3", 0)
            j4 = tool_input.get("j4", 0)
            j5 = tool_input.get("j5", 0)
            j6 = tool_input.get("j6", 0)
            # Map to existing p4-robot.ino command format:
            # "servo j1 <j1> <j2> <j3> <j4> <j5> <j6>"
            return f"servo j1 {j1:.3f} {j2:.3f} {j3:.3f} {j4:.3f} {j5:.3f} {j6:.3f}"
        elif tool_name == "servoStart":
            return "servo start"
        elif tool_name == "servoEnd":
            return "servo end"
        elif tool_name == "getStatus":
            return "status"
        else:
            return ""

    async def run_turn(self, user_text: str) -> dict:
        """
        Send user text to Claude, process tool calls, return response.
        Returns: {"text": "...", "tools_executed": [...], "error": "..."}
        """
        result = {"text": "", "tools_executed": [], "error": ""}

        # Add user message
        self.messages.append({"role": "user", "content": user_text})

        try:
            # Call Claude (may loop for tool use)
            while True:
                response = await self.client.messages.create(
                    model="claude-sonnet-4-6",
                    max_tokens=1024,
                    tools=ROBOT_TOOLS,
                    system=SYSTEM_PROMPT,
                    messages=[m for m in self.messages if m["role"] != "system"],
                )

                # Handle stop reason
                if response.stop_reason == "tool_use":
                    # Process all tool use blocks
                    tool_results = []
                    for block in response.content:
                        if block.type == "text":
                            if block.text.strip():
                                result["text"] += block.text + "\n"
                        elif block.type == "tool_use":
                            tool_name = block.name
                            tool_input = block.input
                            log.info("[%s] Tool call: %s(%s)", self.session_id[:6], tool_name, json.dumps(tool_input, ensure_ascii=False))

                            # Convert to UDP command
                            cmd = self.tool_to_cmd(tool_name, tool_input)

                            if cmd:
                                self.send_udp(cmd)
                                result["tools_executed"].append({"tool": tool_name, "cmd": cmd, "input": tool_input})
                                tool_result_text = f"Command sent: {cmd}"
                            elif tool_name == "say":
                                result["text"] += tool_input.get("text", "") + "\n"
                                tool_result_text = f"TTS: {tool_input.get('text', '')}"
                            else:
                                tool_result_text = f"Unknown tool: {tool_name}"

                            tool_results.append({
                                "type": "tool_result",
                                "tool_use_id": block.id,
                                "content": tool_result_text,
                            })

                    # Add assistant + tool results to history
                    self.messages.append({"role": "assistant", "content": response.content})
                    self.messages.append({"role": "user", "content": tool_results})

                elif response.stop_reason == "end_turn":
                    for block in response.content:
                        if block.type == "text":
                            result["text"] += block.text + "\n"
                    self.messages.append({"role": "assistant", "content": response.content})
                    break

                else:
                    result["error"] = f"Unexpected stop reason: {response.stop_reason}"
                    break

        except RateLimitError as e:
            result["error"] = f"Claude API rate limited: {e}"
            log.error("[%s] Rate limit: %s", self.session_id[:6], e)
        except APIError as e:
            result["error"] = f"Claude API error: {e}"
            log.error("[%s] API error: %s", self.session_id[:6], e)
        except Exception as e:
            result["error"] = f"Unexpected error: {e}"
            log.exception("[%s] Unexpected error", self.session_id[:6])

        return result


# ── WebSocket Server ─────────────────────────────────────────────────────
class AgentServer:
    """WebSocket server that bridges ESP32 voice commands to Claude."""

    def __init__(self, host: str, port: int, robot_ip: str, robot_port: int):
        self.host = host
        self.port = port
        self.robot_ip = robot_ip
        self.robot_port = robot_port
        self.anthropic: Optional[AsyncAnthropic] = None

    async def handle(self, ws: WebSocketServerProtocol):
        """Handle one ESP32-P4 WebSocket connection."""
        sid = uuid.uuid4().hex[:8]
        peer = ws.remote_address
        log.info("[%s] Connected from %s", sid, peer)

        session = AgentSession(
            client=self.anthropic,
            robot_ip=self.robot_ip,
            robot_port=self.robot_port,
            session_id=sid,
        )

        try:
            async for raw in ws:
                try:
                    msg = json.loads(raw)
                except json.JSONDecodeError:
                    log.warning("[%s] Invalid JSON: %s", sid, raw[:100])
                    continue

                msg_type = msg.get("type", "")
                text = msg.get("text", "").strip()

                if not text:
                    log.info("[%s] Empty text, skipping", sid)
                    continue

                if msg_type == "voice_command":
                    log.info("[%s] Voice: %s", sid, text)

                    # Send acknowledgment
                    await ws.send(json.dumps({
                        "type": "status",
                        "status": "processing",
                        "session": sid,
                    }))

                    # Run Claude
                    result = await session.run_turn(text)

                    # Send result back to ESP32
                    response = {
                        "type": "agent_response",
                        "session": sid,
                        "text": result["text"].strip(),
                        "tools_executed": result["tools_executed"],
                    }
                    if result["error"]:
                        response["error"] = result["error"]
                        response["status"] = "error"
                    else:
                        response["status"] = "ok"

                    await ws.send(json.dumps(response, ensure_ascii=False))
                    log.info("[%s] Response: %s", sid, result["text"][:80].strip())

                elif msg_type == "ping":
                    await ws.send(json.dumps({"type": "pong", "session": sid}))

                else:
                    log.warning("[%s] Unknown message type: %s", sid, msg_type)

        except websockets.exceptions.ConnectionClosed:
            log.info("[%s] Connection closed", sid)
        except Exception as e:
            log.exception("[%s] Handler error", sid)
        finally:
            if session.udp_sock:
                session.udp_sock.close()

    async def start(self):
        """Start WebSocket server."""
        # Init Anthropic client
        self.anthropic = AsyncAnthropic()
        log.info("Anthropic client initialized")

        log.info("WebSocket server listening on ws://%s:%d", self.host, self.port)
        log.info("Robot UDP target: %s:%d", self.robot_ip, self.robot_port)
        log.info("Ready — waiting for ESP32-P4 voice commands...")

        async with websockets.serve(self.handle, self.host, self.port, ping_interval=30, ping_timeout=10):
            await asyncio.Future()  # run forever


# ── CLI ──────────────────────────────────────────────────────────────────
def main():
    parser = ArgumentParser(description="Claude Agent Bridge for ESP32-P4 Voice Robot Control")
    parser.add_argument("--host", default="0.0.0.0", help="WebSocket listen address")
    parser.add_argument("--port", type=int, default=9000, help="WebSocket listen port")
    parser.add_argument("--robot-ip", default="192.168.58.100", help="ESP32-P4 IP address")
    parser.add_argument("--robot-port", type=int, default=20008, help="ESP32-P4 UDP command port")
    args = parser.parse_args()

    server = AgentServer(
        host=args.host,
        port=args.port,
        robot_ip=args.robot_ip,
        robot_port=args.robot_port,
    )

    try:
        asyncio.run(server.start())
    except KeyboardInterrupt:
        log.info("Shutting down...")
    except Exception as e:
        log.exception("Fatal error")
        sys.exit(1)


if __name__ == "__main__":
    main()
