---
name: Bug Report
description: 报告一个 bug
labels: [bug]
body:
  - type: dropdown
    attributes:
      label: 所属模块
      options:
        - firmware/s3-exo (固件)
        - visualizer (可视化)
        - voice-ai (AI Agent)
        - hardware (硬件设计)
        - docs (文档)
  - type: textarea
    attributes:
      label: 描述
      placeholder: 清晰描述 bug 是什么
  - type: textarea
    attributes:
      label: 复现步骤
      placeholder: |
        1. 打开 ...
        2. 点击 ...
        3. 出现错误
  - type: textarea
    attributes:
      label: 期望行为
  - type: textarea
    attributes:
      label: 实际行为
  - type: textarea
    attributes:
      label: 日志 / 截图
