# Class 2

## Vulkan 简介

- Vulkan标准中的特性不一定被支持！必须经过测试
- 扩展（extension）：各个平台都有扩展，扩展可能被吸收到核心特性中
- 安卓平台：高通兼容较好，华为/联发科一般

### 传统RHI与现代RHI

#### 传统RHI

- 隐式的Rendering Context
- 驱动比较重，对GPU掌控少

### 现代RHI

- D3D12/Vulkan/Metal
- 依赖程序/开发者，需要开发者处理同步与内存同步
- 多线程友好
- 几乎完全掌控GPU

### Vulkan

- Command Buffer
- 手动申请内存与管理内存
- 尽量按照标准方式实现，没有默认值

## 程序流水线

### 初始化

- 创建窗口
- 初始化vulkan
- 创建swapchain

### 渲染主循环

- 输入
  - 几何信息
  - Uniform

- 输出
- 逻辑
  - 着色器

- CPU端的业务逻辑
- CPU数据拷贝到GPU
- 用Command Buffer将指令给到GPU

## 初始化Vulkan

- Application > Instance > Physical Device > Device > Queue
  - 为什么多个逻辑设备：队列类型不同（极为少见）

- Vulkan Layer：功能被拆分到了多个layer中
  - Layer为Vulkan提供了一个可以重写函数的方式

1. 创建实例
2. 根据能力，查找合适的物理设备
   - 可选特性：几何着色器，细分着色器，压缩纹理etc
   - 查找队列簇
3. 从Vulkan实例创建Surface与Swapchain
   - 查询队列簇支持Surface和Graphics
   - 选择Swapchain属性：颜色空间，刷新方式，缓冲数量
   - 获取对应的Image和Image View
   - 创建对应framebuffer

...

## 顶点数据

