---
name: Feature Request
description: 建议一个新功能
labels: [enhancement]
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
      label: 功能描述
      placeholder: 你想要什么功能？
  - type: textarea
    attributes:
      label: 为什么需要
      placeholder: 这个功能解决什么问题？
  - type: textarea
    attributes:
      label: 实现思路（可选）
