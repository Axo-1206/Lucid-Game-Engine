# Lucid Game Engine — File Structure

```
Lucid-Game-Engine/
├── assets/
│   ├── scenes/
│   │   └── demo.lscene
│   └── .gitkeep
├── docs/
│   ├── archive/
│   │   ├── designs/
│   │   │   ├── AssetPipeline.md
│   │   │   ├── BuildSystem.md
│   │   │   ├── ControlPanelDesign.md
│   │   │   ├── EcsSerialization.md
│   │   │   ├── ExecutionPlan.md
│   │   │   ├── LucidEngineOverview.md
│   │   │   ├── LucidEngineSecurity.md
│   │   │   ├── LucidFileFormats.md
│   │   │   ├── LucidVersionControll.md
│   │   │   └── SecurityKeyGuide.md
│   │   ├── kernel/
│   │   │   ├── kernel_api.md
│   │   │   └── kernel_implementation.md
│   │   ├── EngineArchitecture.md
│   │   ├── MasterPlan.md
│   │   └── NOTE.md
│   ├── implementationPlans/
│   │   └── kernelImplementationPlan.md
│   ├── Decision.md
│   ├── FileStructure.md
│   └── LucidGraphFormat.md
├── editor/
│   ├── include/
│   │   └── editor/
│   │       └── editor_app.h
│   ├── src/
│   │   └── editor_app.cpp
│   └── CMakeLists.txt
├── editor_main/
│   ├── CMakeLists.txt
│   └── main.cpp
├── engine/
│   ├── include/
│   │   └── engine/
│   │       ├── camera.h
│   │       ├── camera2d.h
│   │       ├── clock.h
│   │       ├── component_tags.h
│   │       ├── entity.h
│   │       ├── input.h
│   │       ├── linear_allocator.h
│   │       ├── log.h
│   │       ├── render_list.h
│   │       ├── render_types.h
│   │       ├── renderer.h
│   │       ├── scene.h
│   │       ├── system_context.h
│   │       ├── systems.h
│   │       ├── texture.h
│   │       └── window.h
│   ├── src/
│   │   ├── core/
│   │   │   ├── clock.cpp
│   │   │   ├── linear_allocator.cpp
│   │   │   └── log.cpp
│   │   ├── ecs/
│   │   │   └── entity.cpp
│   │   ├── input/
│   │   │   ├── input_glfw.cpp
│   │   │   ├── input_glfw.h
│   │   │   └── input_state.cpp
│   │   ├── platform/
│   │   │   └── window_glfw.cpp
│   │   ├── render/
│   │   │   └── backend_gl/
│   │   │       ├── renderer_gl.cpp
│   │   │       ├── renderer_gl.h
│   │   │       ├── shader_gl.cpp
│   │   │       ├── shader_gl.h
│   │   │       ├── sprite_batch_gl.cpp
│   │   │       └── sprite_batch_gl.h
│   │   ├── scene/
│   │   │   ├── scene.cpp
│   │   │   ├── scene_loader.cpp
│   │   │   └── scene_saver.cpp
│   │   └── system/
│   │       ├── hierarchy_system.cpp
│   │       ├── movement_system.cpp
│   │       ├── render_list.cpp
│   │       ├── render_system.cpp
│   │       └── system_context.cpp
│   └── CMakeLists.txt
├── externals/
│   ├── include/
│   │   ├── glad/
│   │   │   └── glad.h
│   │   └── KHR/
│   │       └── khrplatform.h
│   ├── nlohmann/
│   │   └── json.hpp
│   ├── src/
│   │   └── glad.c
│   ├── stb/
│   │   └── stb_image.h
│   └── CMakeLists.txt
├── game/
│   ├── CMakeLists.txt
│   └── main.cpp
├── other/
│   └── notes/
│       ├── Step0.md
│       ├── Step1.md
│       ├── Step2.md
│       ├── Step3.md
│       ├── Step4.md
│       ├── Step5.md
│       ├── Step6.md
│       └── Step7.md
├── runtime/
│   ├── include/
│   │   └── lucid/
│   │       ├── cellvalue.h
│   │       ├── column.h
│   │       ├── function.h
│   │       ├── row_ref.h
│   │       ├── runtime.h
│   │       ├── table.h
│   │       ├── types.h
│   │       └── version.h
│   ├── src/
│   │   ├── cellvalue.cpp
│   │   ├── column.cpp
│   │   ├── function.cpp
│   │   ├── row_ref.cpp
│   │   ├── runtime.cpp
│   │   ├── table.cpp
│   │   ├── types.cpp
│   │   └── version.cpp
│   └── CMakeLists.txt
├── tests/
│   ├── engine/
│   │   ├── test_clock.cpp
│   │   ├── test_entity.cpp
│   │   ├── test_input.cpp
│   │   ├── test_linear_allocator.cpp
│   │   ├── test_log.cpp
│   │   ├── test_scene.cpp
│   │   └── test_systems.cpp
│   ├── runtime/
│   │   ├── test_cellvalue.cpp
│   │   ├── test_column.cpp
│   │   ├── test_row_ref.cpp
│   │   ├── test_runtime.cpp
│   │   └── test_table.cpp
│   └── CMakeLists.txt
├── .gitignore
├── CMakeLists.txt
├── LICENSE
└── README.md
```
