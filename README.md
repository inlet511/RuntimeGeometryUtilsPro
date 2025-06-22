# RuntimeGeometryUtilsPro

Forked from Gradientspace, Original URL: https://github.com/gradientspace/UnrealMeshProcessingTools

It's in this subfolder: UE4.26/RuntimeGeometryDemo/Plugins/RuntimeGeometryUtils

https://github.com/inlet511/RuntimeGeometryUtils 的增强版

## 开发说明
### FillHole
参考了MeshRepaireFunctions的FillAllMeshHoles函数

### SimplifyMesh
第一参考是SimplifyMeshOp的CalculateResult函数, 以及SimplifyMeshTool
如果要使用ESimplifyType::UEStandard, 则必须提供 OriginalMeshDescription以及MeshReduction
MeshReduction参考

第二参考是GeometryScripting的MeshSimplifyFunctions类
- ApplySimplifyToPlanar 
- ApplySimplifyToPolygroupTopology
- ApplySimplifyToTriangleCount
- ApplySimplifyToVertexCount
- ApplySimplifyToTolerance


## 更新记录
- Updated for Unreal Engine 5.2
- Updated for Unreal Engine 5.3
- 添加了FillHole功能
- 添加了SimplifyMesh函数，可以选择简化的方式
  - 不能选择ESimplifyType::UEStandard，如果选择该选项，需要提供MeshDescription, 但目前提供MeshDescrition的方法有问题
- 添加了导出Obj的功能
- Updated for Unreal Engine 5.5


