<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh" sourcelanguage="en">
<context>
    <name>BlenderRenderer</name>
    <message>
        <location filename="../../src/blender_renderer.cpp" line="107"/>
        <source>Starting Blender...</source>
        <translation>正在启动 Blender...</translation>
    </message>
</context>
<context>
    <name>DependencyPacker</name>
    <message>
        <location filename="../../src/dependency_packer.cpp" line="106"/>
        <source>Blender Asset Tracer not found at &quot;%1&quot; (set batPath in distributed.json)</source>
        <translation>在 &quot;%1&quot; 找不到 Blender Asset Tracer（请在 distributed.json 中设置 batPath）</translation>
    </message>
    <message>
        <location filename="../../src/dependency_packer.cpp" line="159"/>
        <source>Cannot prepare the dependency cache in %1</source>
        <translation>无法在 %1 准备依赖缓存</translation>
    </message>
    <message>
        <location filename="../../src/dependency_packer.cpp" line="215"/>
        <source>Dependency packing failed (exit %1): %2</source>
        <translation>依赖打包失败（退出码 %1）：%2</translation>
    </message>
    <message>
        <location filename="../../src/dependency_packer.cpp" line="223"/>
        <source>Packed %1 file(s) for remote workers</source>
        <translation>已为远端节点打包 %1 个文件</translation>
    </message>
    <message>
        <location filename="../../src/dependency_packer.cpp" line="226"/>
        <source>Dependency packing wrote an unreadable manifest</source>
        <translation>依赖打包生成的清单无法读取</translation>
    </message>
</context>
<context>
    <name>DropFileTip</name>
    <message>
        <location filename="../../ui/drop_file_tip.ui" line="14"/>
        <source>Form</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../ui/drop_file_tip.ui" line="102"/>
        <location filename="../../src/drop_file_tip.cpp" line="14"/>
        <source>Drop your Blender Projects here.</source>
        <translation>将你的 Blender 工程文件拖拽至此处.</translation>
    </message>
</context>
<context>
    <name>FileBar</name>
    <message>
        <location filename="../../src/file_bar.cpp" line="42"/>
        <source>Click to edit frame range</source>
        <translation>点击修改帧范围</translation>
    </message>
    <message>
        <location filename="../../src/file_bar.cpp" line="41"/>
        <source>Click to rename project</source>
        <translation>点击重命名项目</translation>
    </message>
</context>
<context>
    <name>FrameRangeDialog</name>
    <message>
        <location filename="../../ui/frame_range_dialog.ui" line="26"/>
        <source>Set Frame Range</source>
        <translation>设置帧范围</translation>
    </message>
    <message>
        <location filename="../../ui/frame_range_dialog.ui" line="136"/>
        <source>Frame Start:</source>
        <translation>起始帧: </translation>
    </message>
    <message>
        <location filename="../../ui/frame_range_dialog.ui" line="171"/>
        <source>Frame End:</source>
        <translation>结束帧: </translation>
    </message>
    <message>
        <location filename="../../ui/frame_range_dialog.ui" line="206"/>
        <source>Frame Step:</source>
        <translation>帧步长: </translation>
    </message>
    <message>
        <location filename="../../ui/frame_range_dialog.ui" line="276"/>
        <source>OK</source>
        <translation>确定</translation>
    </message>
    <message>
        <location filename="../../ui/frame_range_dialog.ui" line="298"/>
        <source>Cancel</source>
        <translation>取消</translation>
    </message>
</context>
<context>
    <name>NodeAgent</name>
    <message>
        <location filename="../../src/node_agent.cpp" line="106"/>
        <source>Node discovery: UDP port %1 is unavailable, only configured peers are found</source>
        <translation>节点发现：UDP 端口 %1 不可用，只能找到配置里的节点</translation>
    </message>
    <message>
        <location filename="../../src/node_agent.cpp" line="233"/>
        <source>Handing back the lease from %1: this machine is needed</source>
        <translation>本机需要使用，已把 %1 的任务归还</translation>
    </message>
    <message>
        <location filename="../../src/node_agent.cpp" line="281"/>
        <source>Rendering for %1 (%2:%3)</source>
        <translation>正在为 %1 渲染（%2:%3）</translation>
    </message>
    <message>
        <location filename="../../src/node_agent.cpp" line="307"/>
        <source>%1 is unreachable</source>
        <translation>无法连接 %1</translation>
    </message>
    <message>
        <location filename="../../src/node_agent.cpp" line="311"/>
        <source>%1 rejected this machine, check that both use the same token</source>
        <translation>%1 拒绝了本机，请检查两台机器的 token 是否一致</translation>
    </message>
</context>
<context>
    <name>NodeList</name>
    <message numerus="yes">
        <location filename="../../src/node_panel.cpp" line="150"/>
        <source>+%n more node(s)</source>
        <translation>
            <numerusform>还有 %n 个节点</numerusform>
        </translation>
    </message>
</context>
<context>
    <name>NodePanel</name>
    <message>
        <location filename="../../src/node_panel.cpp" line="176"/>
        <source>Render Nodes</source>
        <translation>渲染节点</translation>
    </message>
    <message>
        <location filename="../../src/node_panel.cpp" line="181"/>
        <source>Accept work</source>
        <translation>接受远端任务</translation>
    </message>
    <message>
        <location filename="../../src/node_panel.cpp" line="184"/>
        <source>Let other Rendrop nodes render on this machine while it is idle</source>
        <translation>本机空闲时允许其他 Rendrop 节点在这里渲染</translation>
    </message>
    <message>
        <location filename="../../src/node_panel.cpp" line="214"/>
        <source>Distributed rendering is off.
Set &quot;enabled&quot; in distributed.json to join a render group.</source>
        <translation>分布式渲染未开启。
在 distributed.json 中设置 &quot;enabled&quot; 以加入渲染组。</translation>
    </message>
    <message>
        <location filename="../../src/node_panel.cpp" line="229"/>
        <source>For %1 %2/%3</source>
        <translation>为 %1 渲染 %2/%3</translation>
    </message>
    <message>
        <location filename="../../src/node_panel.cpp" line="230"/>
        <source>For %1</source>
        <translation>为 %1 渲染</translation>
    </message>
    <message>
        <location filename="../../src/node_panel.cpp" line="235"/>
        <source>Rendering, sharing frames</source>
        <translation>渲染中，分发帧</translation>
    </message>
    <message>
        <location filename="../../src/node_panel.cpp" line="235"/>
        <source>Rendering own queue</source>
        <translation>渲染本机队列</translation>
    </message>
    <message>
        <location filename="../../src/node_panel.cpp" line="239"/>
        <location filename="../../src/node_panel.cpp" line="283"/>
        <source>Busy</source>
        <translation>忙碌</translation>
    </message>
    <message>
        <location filename="../../src/node_panel.cpp" line="243"/>
        <location filename="../../src/node_panel.cpp" line="278"/>
        <source>Not accepting work</source>
        <translation>不接受任务</translation>
    </message>
    <message>
        <location filename="../../src/node_panel.cpp" line="247"/>
        <source>No Blender selected</source>
        <translation>未选择 Blender</translation>
    </message>
    <message>
        <location filename="../../src/node_panel.cpp" line="251"/>
        <location filename="../../src/node_panel.cpp" line="288"/>
        <source>Idle</source>
        <translation>空闲</translation>
    </message>
    <message>
        <location filename="../../src/node_panel.cpp" line="255"/>
        <source>Off</source>
        <translation>关闭</translation>
    </message>
    <message>
        <location filename="../../src/node_panel.cpp" line="268"/>
        <source>Sharing frames</source>
        <translation>分发帧</translation>
    </message>
    <message>
        <location filename="../../src/node_panel.cpp" line="273"/>
        <source>Rendering</source>
        <translation>渲染中</translation>
    </message>
    <message>
        <location filename="../../src/node_panel.cpp" line="296"/>
        <source>Looking for other nodes on the LAN...</source>
        <translation>正在局域网内查找其他节点...</translation>
    </message>
    <message>
        <location filename="../../src/node_panel.cpp" line="297"/>
        <source>Discovery port unavailable. Add nodes to &quot;peers&quot; in distributed.json.</source>
        <translation>发现端口不可用。请在 distributed.json 的 &quot;peers&quot; 中添加节点。</translation>
    </message>
</context>
<context>
    <name>RenameDialog</name>
    <message>
        <location filename="../../ui/rename_dialog.ui" line="20"/>
        <source>Rename Project</source>
        <translation>重命名项目</translation>
    </message>
    <message>
        <location filename="../../ui/rename_dialog.ui" line="73"/>
        <source>New Name:</source>
        <translation>新名称:</translation>
    </message>
    <message>
        <location filename="../../ui/rename_dialog.ui" line="125"/>
        <source>OK</source>
        <translation>确定</translation>
    </message>
    <message>
        <location filename="../../ui/rename_dialog.ui" line="147"/>
        <source>Cancel</source>
        <translation>取消</translation>
    </message>
</context>
<context>
    <name>RenderQueue</name>
    <message>
        <location filename="../../src/render_queue.cpp" line="623"/>
        <source>Worker %1 took frames %2-%3</source>
        <translation>节点 %1 领取了第 %2-%3 帧</translation>
    </message>
    <message>
        <location filename="../../src/render_queue.cpp" line="668"/>
        <location filename="../../src/render_queue.cpp" line="686"/>
        <source>unknown</source>
        <translation>未知</translation>
    </message>
    <message>
        <location filename="../../src/render_queue.cpp" line="678"/>
        <source>Worker %1 failed: %2</source>
        <translation>节点 %1 失败：%2</translation>
    </message>
    <message>
        <location filename="../../src/render_queue.cpp" line="696"/>
        <source>Worker %1 is busy and returned its frames</source>
        <translation>节点 %1 忙碌，已归还帧</translation>
    </message>
    <message>
        <location filename="../../src/render_queue.cpp" line="783"/>
        <source>Lease for worker %1 expired, frames returned to the queue</source>
        <translation>节点 %1 的租约超时，帧已退回队列</translation>
    </message>
</context>
<context>
    <name>Widget</name>
    <message>
        <location filename="../../src/widget.cpp" line="295"/>
        <location filename="../../src/widget.cpp" line="303"/>
        <location filename="../../src/widget.cpp" line="513"/>
        <location filename="../../src/widget.cpp" line="543"/>
        <location filename="../../src/widget.cpp" line="555"/>
        <source>Warning</source>
        <translation>警告</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="296"/>
        <source>Some files are still rendering. Are you sure to exit?</source>
        <translation>一些文件仍在渲染中，确定要退出吗？</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="304"/>
        <source>Some files are still loading. Are you sure to exit?</source>
        <translation>一些文件仍在加载中，确定要退出吗？</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="544"/>
        <source> is still loading. Are you sure to delete?</source>
        <translation> 正在加载中，确定要删除吗？</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="556"/>
        <source> is still rendering. Are you sure to delete?</source>
        <translation> 正在渲染中，确定要删除吗？</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="658"/>
        <source>Open File</source>
        <translation>打开文件</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="660"/>
        <source>Blender Projects (*.blend)</source>
        <translation>Blender 工程文件 (*.blend)</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="676"/>
        <source>Select Blender Version</source>
        <translation>选择 Blender 版本</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="678"/>
        <source>Blender Executable (blender.exe)</source>
        <translation>Blender 主程序 (blender.exe)</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="685"/>
        <location filename="../../src/widget.cpp" line="689"/>
        <location filename="../../src/widget.cpp" line="697"/>
        <location filename="../../src/widget.cpp" line="718"/>
        <source>Error</source>
        <translation>错误</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="685"/>
        <source>Cannot open specified path.</source>
        <translation>打开指定的路径时出现错误.</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="689"/>
        <source>Failed to start Blender.</source>
        <translation>Blender 启动失败.</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="693"/>
        <source>Note</source>
        <translation>提示</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="693"/>
        <source>Selected Blender version already exists.</source>
        <translation>选择的 Blender 版本已存在.</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="697"/>
        <location filename="../../src/widget.cpp" line="718"/>
        <source>Failed to save version configuration file.</source>
        <translation>保存配置文件时出现错误.</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="645"/>
        <source>Stop Render</source>
        <translation>停止渲染</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="645"/>
        <source>Start Render</source>
        <translation>开始渲染</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="75"/>
        <location filename="../../src/widget.cpp" line="750"/>
        <source>-- Add A Blender Version --</source>
        <translation>-- 添加 Blender 版本 --</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="829"/>
        <source>Ready to render.</source>
        <translation>准备渲染.</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="514"/>
        <source>Language settings will take effect after restarting the program. Restart now?</source>
        <translation>语言设置将在重启程序后生效，是否重启程序？</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="819"/>
        <location filename="../../src/widget.cpp" line="820"/>
        <source>Please add a Blender version first.</source>
        <translation>请先添加一个 Blender 版本.</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="824"/>
        <source>Add a Blender project to get started.</source>
        <translation>添加一个 Blender 项目以开始.</translation>
    </message>
    <message>
        <location filename="../../src/widget.cpp" line="825"/>
        <source>Drop your Blender Projects here.</source>
        <translation>将你的 Blender 工程文件拖拽至此处.</translation>
    </message>
</context>
<context>
    <name>fileBar</name>
    <message>
        <location filename="../../ui/file_bar.ui" line="26"/>
        <source>Form</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../ui/file_bar.ui" line="239"/>
        <source>File</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../ui/file_bar.ui" line="286"/>
        <location filename="../../ui/file_bar.ui" line="320"/>
        <location filename="../../ui/file_bar.ui" line="363"/>
        <source>...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../ui/file_bar.ui" line="429"/>
        <source>0/1000 (0%)</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>main</name>
    <message>
        <location filename="../../src/main.cpp" line="65"/>
        <source>Warning</source>
        <translation>警告</translation>
    </message>
    <message>
        <location filename="../../src/main.cpp" line="64"/>
        <source>The application is already running.</source>
        <translation>该应用已在运行.</translation>
    </message>
</context>
<context>
    <name>widget</name>
    <message>
        <location filename="../../ui/widget.ui" line="26"/>
        <location filename="../../ui/widget.ui" line="117"/>
        <source>Rendrop</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../ui/widget.ui" line="212"/>
        <source>English</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../ui/widget.ui" line="217"/>
        <source>中文</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../ui/widget.ui" line="334"/>
        <source>Select Blender Version</source>
        <translation>选择 Blender 版本</translation>
    </message>
    <message>
        <location filename="../../ui/widget.ui" line="713"/>
        <source>4125 / 51200</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../ui/widget.ui" line="751"/>
        <source>Frames</source>
        <translation>总帧数</translation>
    </message>
    <message>
        <location filename="../../ui/widget.ui" line="833"/>
        <source>5 / 6</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../ui/widget.ui" line="871"/>
        <source>Projects</source>
        <translation>总文件数</translation>
    </message>
    <message>
        <location filename="../../ui/widget.ui" line="1132"/>
        <source>Output</source>
        <translation>输出</translation>
    </message>
    <message>
        <location filename="../../ui/widget.ui" line="1226"/>
        <source>Start Render</source>
        <translation type="unfinished">开始渲染</translation>
    </message>
</context>
</TS>
