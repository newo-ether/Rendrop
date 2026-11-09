<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh" sourcelanguage="en">
<context>
    <name>BlenderRenderer</name>
    <message>
        <location filename="../../src/core/blender_renderer.cpp" line="107"/>
        <source>Starting Blender...</source>
        <translation>正在启动 Blender...</translation>
    </message>
</context>
<context>
    <name>DependencyPacker</name>
    <message>
        <location filename="../../src/core/dependency_packer.cpp" line="106"/>
        <source>Blender Asset Tracer not found at &quot;%1&quot; (set batPath in distributed.json)</source>
        <translation>在 &quot;%1&quot; 找不到 Blender Asset Tracer（请在 distributed.json 中设置 batPath）</translation>
    </message>
    <message>
        <location filename="../../src/core/dependency_packer.cpp" line="159"/>
        <source>Cannot prepare the dependency cache in %1</source>
        <translation>无法在 %1 准备依赖缓存</translation>
    </message>
    <message>
        <location filename="../../src/core/dependency_packer.cpp" line="215"/>
        <source>Dependency packing failed (exit %1): %2</source>
        <translation>依赖打包失败（退出码 %1）：%2</translation>
    </message>
    <message>
        <location filename="../../src/core/dependency_packer.cpp" line="223"/>
        <source>Packed %1 file(s) for remote workers</source>
        <translation>已为远端节点打包 %1 个文件</translation>
    </message>
    <message>
        <location filename="../../src/core/dependency_packer.cpp" line="226"/>
        <source>Dependency packing wrote an unreadable manifest</source>
        <translation>依赖打包生成的清单无法读取</translation>
    </message>
</context>
<context>
    <name>DropFileTip</name>
    <message>
        <location filename="../../src/ui/forms/drop_file_tip.ui" line="14"/>
        <source>Form</source>
        <translation>Form</translation>
    </message>
    <message>
        <location filename="../../src/ui/forms/drop_file_tip.ui" line="85"/>
        <location filename="../../src/ui/widgets/drop_file_tip.cpp" line="17"/>
        <source>Drop your Blender Projects here.</source>
        <translation>将你的 Blender 工程文件拖拽至此处.</translation>
    </message>
</context>
<context>
    <name>FileBar</name>
    <message>
        <location filename="../../src/ui/widgets/file_bar.cpp" line="48"/>
        <source>Click to edit frame range</source>
        <translation>点击修改帧范围</translation>
    </message>
    <message>
        <location filename="../../src/ui/widgets/file_bar.cpp" line="127"/>
        <source>Click to open the project</source>
        <translation>点击查看工程详情</translation>
    </message>
    <message numerus="yes">
        <location filename="../../src/ui/widgets/file_bar.cpp" line="464"/>
        <source> · %n node(s)</source>
        <translation>
            <numerusform> · %n 个节点</numerusform>
        </translation>
    </message>
    <message>
        <location filename="../../src/ui/widgets/file_bar.cpp" line="47"/>
        <source>Click to rename project</source>
        <translation>点击重命名项目</translation>
    </message>
    <message>
        <location filename="../../src/ui/widgets/file_bar.cpp" line="78"/>
        <source>Drag to reorder</source>
        <translation>拖动排序</translation>
    </message>
</context>
<context>
    <name>FrameGrid</name>
    <message>
        <location filename="../../src/ui/pages/project_page.cpp" line="593"/>
        <source>No finished frames yet.</source>
        <translation>还没有渲染完成的帧。</translation>
    </message>
    <message numerus="yes">
        <location filename="../../src/ui/pages/project_page.cpp" line="629"/>
        <source>%n frame(s)</source>
        <translation>
            <numerusform>%n 帧</numerusform>
        </translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/project_page.cpp" line="796"/>
        <source>Frame %1, rendered by %2</source>
        <translation>第 %1 帧，由 %2 渲染</translation>
    </message>
</context>
<context>
    <name>FrameRangeDialog</name>
    <message>
        <location filename="../../src/ui/forms/frame_range_dialog.ui" line="26"/>
        <source>Set Frame Range</source>
        <translation>设置帧范围</translation>
    </message>
    <message>
        <location filename="../../src/ui/forms/frame_range_dialog.ui" line="52"/>
        <source>Frame Start:</source>
        <translation>起始帧: </translation>
    </message>
    <message>
        <location filename="../../src/ui/forms/frame_range_dialog.ui" line="87"/>
        <source>Frame End:</source>
        <translation>结束帧: </translation>
    </message>
    <message>
        <location filename="../../src/ui/forms/frame_range_dialog.ui" line="122"/>
        <source>Frame Step:</source>
        <translation>帧步长: </translation>
    </message>
    <message>
        <location filename="../../src/ui/forms/frame_range_dialog.ui" line="192"/>
        <source>OK</source>
        <translation>确定</translation>
    </message>
    <message>
        <location filename="../../src/ui/forms/frame_range_dialog.ui" line="214"/>
        <source>Cancel</source>
        <translation>取消</translation>
    </message>
</context>
<context>
    <name>NodeAgent</name>
    <message>
        <location filename="../../src/farm/node_agent.cpp" line="106"/>
        <source>Node discovery: UDP port %1 is unavailable, only configured peers are found</source>
        <translation>节点发现：UDP 端口 %1 不可用，只能找到配置里的节点</translation>
    </message>
    <message>
        <location filename="../../src/farm/node_agent.cpp" line="233"/>
        <source>Handing back the lease from %1: this machine is needed</source>
        <translation>本机需要使用，已把 %1 的任务归还</translation>
    </message>
    <message>
        <location filename="../../src/farm/node_agent.cpp" line="281"/>
        <source>Rendering for %1 (%2:%3)</source>
        <translation>正在为 %1 渲染（%2:%3）</translation>
    </message>
    <message>
        <location filename="../../src/farm/node_agent.cpp" line="307"/>
        <source>%1 is unreachable</source>
        <translation>无法连接 %1</translation>
    </message>
    <message>
        <location filename="../../src/farm/node_agent.cpp" line="311"/>
        <source>%1 rejected this machine, check that both use the same token</source>
        <translation>%1 拒绝了本机，请检查两台机器的 token 是否一致</translation>
    </message>
</context>
<context>
    <name>NodeTable</name>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="143"/>
        <source>Node</source>
        <translation>节点</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="143"/>
        <source>Address</source>
        <translation>地址</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="143"/>
        <source>State</source>
        <translation>状态</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="143"/>
        <source>s/frame</source>
        <translation>秒/帧</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="143"/>
        <source>Lease</source>
        <translation>租约</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="143"/>
        <source>Leases done</source>
        <translation>已完成租约</translation>
    </message>
</context>
<context>
    <name>NodesPage</name>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="250"/>
        <source>Load from programs other than Rendrop, averaged over the busy window</source>
        <translation>Rendrop 以外程序的负载，按忙碌窗口取平均</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="253"/>
        <source>Accept work</source>
        <translation>接受远端任务</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="256"/>
        <source>Let other Rendrop nodes render on this machine while it is idle</source>
        <translation>本机空闲时允许其他 Rendrop 节点在这里渲染</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="266"/>
        <source>Render group</source>
        <translation>渲染组</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="271"/>
        <source>Manage nodes...</source>
        <translation>管理节点...</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="275"/>
        <source>Add nodes that broadcast cannot reach, such as Tailscale peers</source>
        <translation>添加广播无法到达的节点，例如 Tailscale 设备</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="337"/>
        <source>Rendering for %1, %2/%3</source>
        <translation>正在为 %1 渲染，%2/%3</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="338"/>
        <source>Rendering for %1</source>
        <translation>正在为 %1 渲染</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="343"/>
        <source>Rendering, sharing frames</source>
        <translation>渲染中，分发帧</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="343"/>
        <source>Rendering own queue</source>
        <translation>渲染本机队列</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="347"/>
        <source>Busy with other programs</source>
        <translation>被其他程序占用</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="350"/>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="451"/>
        <source>Not accepting work</source>
        <translation>不接受任务</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="353"/>
        <source>No Blender selected</source>
        <translation>未选择 Blender</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="357"/>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="461"/>
        <source>Idle</source>
        <translation>空闲</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="360"/>
        <source>Distributed rendering off</source>
        <translation>分布式渲染已关闭</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="391"/>
        <source>n/a</source>
        <translation>无</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="392"/>
        <source>CPU %1%   GPU %2</source>
        <translation>CPU %1%   GPU %2</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="405"/>
        <source>Distributed rendering is off. Turn it on in Settings &gt; Farm to join a render group.</source>
        <translation>分布式渲染已关闭。在 设置 &gt; 集群 中开启后即可加入渲染组。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="436"/>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="477"/>
        <source>Rendering for this machine</source>
        <translation>正在为本机渲染</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="441"/>
        <source>Sharing frames</source>
        <translation>分发帧</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="446"/>
        <source>Rendering</source>
        <translation>渲染中</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="456"/>
        <source>Busy</source>
        <translation>忙碌</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="477"/>
        <source>Worker, waiting</source>
        <translation>工作进程，等待中</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="500"/>
        <source>Looking for other nodes on the LAN...</source>
        <translation>正在局域网内查找其他节点...</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="501"/>
        <source>No other nodes found.</source>
        <translation>没有发现其他节点。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/nodes_page.cpp" line="489"/>
        <source>Discovery port unavailable. Add nodes manually with &quot;Manage nodes...&quot;.</source>
        <translation>发现端口不可用。请通过“管理节点...”手动添加节点。</translation>
    </message>
</context>
<context>
    <name>ProjectPage</name>
    <message>
        <location filename="../../src/ui/pages/project_page.cpp" line="892"/>
        <source>Back</source>
        <translation>返回</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/project_page.cpp" line="896"/>
        <source>Back to the queue</source>
        <translation>返回队列</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/project_page.cpp" line="921"/>
        <source>Open folder</source>
        <translation>打开文件夹</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/project_page.cpp" line="925"/>
        <source>Show the rendered frames in Explorer</source>
        <translation>在资源管理器中查看已渲染的帧</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/project_page.cpp" line="1005"/>
        <source>%1 / %2 frames</source>
        <translation>%1 / %2 帧</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/project_page.cpp" line="1006"/>
        <source>Frames %1-%2, step %3   %4   %5x%6 (%7%)</source>
        <translation>帧 %1-%2，步长 %3   %4   %5x%6 (%7%)</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="102"/>
        <source>Browse</source>
        <translation>浏览</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/project_page.cpp" line="79"/>
        <source>Loading</source>
        <translation>加载中</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/project_page.cpp" line="80"/>
        <source>Queued</source>
        <translation>排队中</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/project_page.cpp" line="81"/>
        <source>Error</source>
        <translation>错误</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/project_page.cpp" line="82"/>
        <source>Rendering</source>
        <translation>渲染中</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/project_page.cpp" line="83"/>
        <source>Finished</source>
        <translation>已完成</translation>
    </message>
</context>
<context>
    <name>QueuePage</name>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="63"/>
        <location filename="../../src/ui/pages/queue_page.cpp" line="783"/>
        <source>Add Blender...</source>
        <translation>添加 Blender...</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="261"/>
        <source>Hide log</source>
        <translation>隐藏日志</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="261"/>
        <source>Show log</source>
        <translation>显示日志</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="508"/>
        <location filename="../../src/ui/pages/queue_page.cpp" line="521"/>
        <source>Warning</source>
        <translation>警告</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="509"/>
        <source> is still loading. Are you sure to delete?</source>
        <translation> 正在加载中，确定要删除吗？</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="522"/>
        <source> is still rendering. Are you sure to delete?</source>
        <translation> 正在渲染中，确定要删除吗？</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="564"/>
        <source>Render failed</source>
        <translation>渲染失败</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="565"/>
        <source>%1 stopped at frame %2 / %3. See the log for details.</source>
        <translation>%1 在第 %2 / %3 帧中止，详情见日志。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="677"/>
        <source>Stop Render</source>
        <translation>停止渲染</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="677"/>
        <source>Start Render</source>
        <translation>开始渲染</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="697"/>
        <source>Render complete: %1 finished.</source>
        <translation>渲染完成：%1 个项目已完成。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="698"/>
        <source>Render ended: %1 finished, %2 failed.</source>
        <translation>渲染结束：%1 个完成，%2 个失败。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="704"/>
        <source>Render complete</source>
        <translation>渲染完成</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="704"/>
        <source>Render ended with errors</source>
        <translation>渲染结束，有项目出错</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="725"/>
        <source>Open File</source>
        <translation>打开文件</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="727"/>
        <source>Blender Projects (*.blend)</source>
        <translation>Blender 工程文件 (*.blend)</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="740"/>
        <source> (%1 at frame %2 / %3)</source>
        <translation>（%1，第 %2 / %3 帧）</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="746"/>
        <source>Render stopped manually</source>
        <translation>已手动停止渲染</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="825"/>
        <source>Projects %1 / %2</source>
        <translation>工程 %1 / %2</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="834"/>
        <source>%1 h %2 min</source>
        <translation>%1 小时 %2 分</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="835"/>
        <source>%1 min</source>
        <translation>%1 分钟</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="836"/>
        <source>ETA %1</source>
        <translation>预计剩余 %1</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="841"/>
        <source>Frames %1 / %2</source>
        <translation>帧 %1 / %2</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="856"/>
        <location filename="../../src/ui/pages/queue_page.cpp" line="857"/>
        <source>Add a Blender version in Settings first.</source>
        <translation>请先在设置中添加 Blender 版本。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="861"/>
        <source>Add a Blender project to get started.</source>
        <translation>添加一个 Blender 项目以开始.</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="862"/>
        <source>Drop your Blender Projects here.</source>
        <translation>将你的 Blender 工程文件拖拽至此处.</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/queue_page.cpp" line="866"/>
        <source>Ready to render.</source>
        <translation>准备渲染.</translation>
    </message>
</context>
<context>
    <name>RenameDialog</name>
    <message>
        <location filename="../../src/ui/forms/rename_dialog.ui" line="20"/>
        <source>Rename Project</source>
        <translation>重命名项目</translation>
    </message>
    <message>
        <location filename="../../src/ui/forms/rename_dialog.ui" line="31"/>
        <source>New Name:</source>
        <translation>新名称:</translation>
    </message>
    <message>
        <location filename="../../src/ui/forms/rename_dialog.ui" line="83"/>
        <source>OK</source>
        <translation>确定</translation>
    </message>
    <message>
        <location filename="../../src/ui/forms/rename_dialog.ui" line="105"/>
        <source>Cancel</source>
        <translation>取消</translation>
    </message>
</context>
<context>
    <name>RenderQueue</name>
    <message>
        <location filename="../../src/core/render_queue.cpp" line="684"/>
        <source>Worker %1 took frames %2-%3</source>
        <translation>节点 %1 领取了第 %2-%3 帧</translation>
    </message>
    <message>
        <location filename="../../src/core/render_queue.cpp" line="729"/>
        <location filename="../../src/core/render_queue.cpp" line="747"/>
        <source>unknown</source>
        <translation>未知</translation>
    </message>
    <message>
        <location filename="../../src/core/render_queue.cpp" line="739"/>
        <source>Worker %1 failed: %2</source>
        <translation>节点 %1 失败：%2</translation>
    </message>
    <message>
        <location filename="../../src/core/render_queue.cpp" line="757"/>
        <source>Worker %1 is busy and returned its frames</source>
        <translation>节点 %1 忙碌，已归还帧</translation>
    </message>
    <message>
        <location filename="../../src/core/render_queue.cpp" line="844"/>
        <source>Lease for worker %1 expired, frames returned to the queue</source>
        <translation>节点 %1 的租约超时，帧已退回队列</translation>
    </message>
</context>
<context>
    <name>SettingsPage</name>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="141"/>
        <location filename="../../src/ui/pages/settings_page.cpp" line="198"/>
        <source>General</source>
        <translation>常规</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="141"/>
        <location filename="../../src/ui/pages/settings_page.cpp" line="225"/>
        <source>Blender</source>
        <translation>Blender</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="141"/>
        <location filename="../../src/ui/pages/settings_page.cpp" line="246"/>
        <source>Farm</source>
        <translation>集群</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="141"/>
        <location filename="../../src/ui/pages/settings_page.cpp" line="292"/>
        <source>Scheduling</source>
        <translation>调度</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="141"/>
        <location filename="../../src/ui/pages/settings_page.cpp" line="308"/>
        <source>Load detection</source>
        <translation>负载检测</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="141"/>
        <location filename="../../src/ui/pages/settings_page.cpp" line="328"/>
        <source>Dependencies</source>
        <translation>依赖</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="141"/>
        <location filename="../../src/ui/pages/settings_page.cpp" line="340"/>
        <source>Cache</source>
        <translation>缓存</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="221"/>
        <location filename="../../src/ui/pages/settings_page.cpp" line="680"/>
        <source>Restore defaults</source>
        <translation>恢复默认设置</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="204"/>
        <source>Language</source>
        <translation>语言</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="204"/>
        <source>Takes effect after a restart.</source>
        <translation>重启后生效。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="215"/>
        <source>Launch at login</source>
        <translation>开机启动</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="216"/>
        <source>Start Rendrop in the system tray when you sign in to Windows.</source>
        <translation>登录 Windows 时在系统托盘中启动 Rendrop。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="219"/>
        <source>Shadows</source>
        <translation>阴影</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="219"/>
        <source>Soft shadows under cards. Turn off to save some drawing work.</source>
        <translation>在卡片下方绘制柔和阴影。关闭后可减少一些绘制开销。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="242"/>
        <source>Parallel loading</source>
        <translation>并行加载</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="243"/>
        <source>Most Blender processes reading added files at once. The rest wait in line.</source>
        <translation>同时读取新加文件的 Blender 进程上限，其余文件排队等待。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="220"/>
        <source>Restore</source>
        <translation>恢复</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="207"/>
        <source>Follow system</source>
        <translation>跟随系统</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="208"/>
        <source>Dark</source>
        <translation>深色</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="209"/>
        <source>Light</source>
        <translation>浅色</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="212"/>
        <source>Appearance</source>
        <translation>外观</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="212"/>
        <source>Follow system switches with the Windows color mode.</source>
        <translation>跟随系统时随 Windows 颜色模式切换。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="222"/>
        <source>Keeps the farm key, manual nodes, Blender versions and language.</source>
        <translation>保留集群密钥、手动节点、Blender 版本和语言。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="226"/>
        <source>Versions you can render with. Pick the active one in the queue toolbar.</source>
        <translation>可用于渲染的版本。在队列工具栏中选择当前使用的版本。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="234"/>
        <location filename="../../src/ui/pages/settings_page.cpp" line="597"/>
        <source>Add Blender...</source>
        <translation>添加 Blender...</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="248"/>
        <source>Distributed rendering</source>
        <translation>分布式渲染</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="249"/>
        <source>Announce this machine on the LAN, share queued frames and render for other nodes.</source>
        <translation>在局域网内广播本机，分发排队的帧，并为其他节点渲染。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="261"/>
        <location filename="../../src/ui/pages/settings_page.cpp" line="405"/>
        <source>Show</source>
        <translation>显示</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="262"/>
        <source>Copy</source>
        <translation>复制</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="263"/>
        <source>New</source>
        <translation>新建</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="264"/>
        <source>Generate a new farm key</source>
        <translation>生成新的集群密钥</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="269"/>
        <source>Farm key</source>
        <translation>集群密钥</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="270"/>
        <source>Shared secret of the render group. Every machine needs the same key.</source>
        <translation>渲染组的共享密钥。每台机器都需要相同的密钥。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="275"/>
        <source>HTTP port</source>
        <translation>HTTP 端口</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="275"/>
        <source>Fixed. Workers and rendropctl connect here.</source>
        <translation>固定值。工作进程和 rendropctl 连接到这个端口。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="277"/>
        <source>Discovery port</source>
        <translation>发现端口</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="277"/>
        <source>UDP port for LAN announcements. Must match on every machine.</source>
        <translation>局域网广播用的 UDP 端口。所有机器必须一致。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="288"/>
        <source>Manual nodes</source>
        <translation>手动节点</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="289"/>
        <source>One host or host:port per line, for nodes broadcast cannot reach (Tailscale, other subnets).</source>
        <translation>每行一个 host 或 host:port，用于广播无法到达的节点（Tailscale、其他子网）。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="294"/>
        <source>Adaptive chunks</source>
        <translation>自适应分块</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="295"/>
        <source>Size each lease from the node&apos;s measured speed.</source>
        <translation>按节点实测速度决定每个租约的大小。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="296"/>
        <location filename="../../src/ui/pages/settings_page.cpp" line="303"/>
        <location filename="../../src/ui/pages/settings_page.cpp" line="318"/>
        <location filename="../../src/ui/pages/settings_page.cpp" line="322"/>
        <source> s</source>
        <translation> 秒</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="297"/>
        <source>Target lease length</source>
        <translation>目标租约时长</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="297"/>
        <source>How long one adaptive lease should take.</source>
        <translation>单个自适应租约的预期耗时。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="298"/>
        <location filename="../../src/ui/pages/settings_page.cpp" line="301"/>
        <source> frames</source>
        <translation> 帧</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="299"/>
        <source>Initial chunk</source>
        <translation>初始分块</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="300"/>
        <source>Frames per lease before a node is measured, or always when adaptive chunks are off.</source>
        <translation>节点测速前每个租约的帧数；关闭自适应分块时始终用这个值。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="302"/>
        <source>Largest chunk</source>
        <translation>最大分块</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="302"/>
        <source>Upper limit for adaptive leases.</source>
        <translation>自适应租约的帧数上限。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="304"/>
        <source>Lease timeout</source>
        <translation>租约超时</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="305"/>
        <source>Frames of a node silent for this long go back to the queue.</source>
        <translation>节点失联超过这个时长后，它的帧会退回队列。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="310"/>
        <source>Load from programs other than Rendrop marks this machine busy, so it stops taking work from other nodes.</source>
        <translation>Rendrop 以外的程序负载过高时，本机标记为忙碌，不再接收其他节点的任务。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="317"/>
        <source>Busy above</source>
        <translation>忙碌阈值</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="317"/>
        <source>Mean CPU or GPU load that marks the machine busy.</source>
        <translation>CPU 或 GPU 平均负载超过这个值即视为忙碌。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="319"/>
        <source>Busy window</source>
        <translation>忙碌窗口</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="319"/>
        <source>Averaging time for the busy check.</source>
        <translation>忙碌判定的取平均时长。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="321"/>
        <source>Idle below</source>
        <translation>空闲阈值</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="321"/>
        <source>Both CPU and GPU must stay under this to be idle again.</source>
        <translation>CPU 和 GPU 都低于这个值才会恢复空闲。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="323"/>
        <source>Idle window</source>
        <translation>空闲窗口</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="323"/>
        <source>Averaging time for the idle check.</source>
        <translation>空闲判定的取平均时长。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="324"/>
        <source> ms</source>
        <translation> 毫秒</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="325"/>
        <source>Sample interval</source>
        <translation>采样间隔</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="330"/>
        <source>Send the .blend only</source>
        <translation>只发送 .blend</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="331"/>
        <source>Collect assets with BAT</source>
        <translation>用 BAT 收集资源</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="333"/>
        <source>Asset handling</source>
        <translation>资源处理</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="334"/>
        <source>With the .blend only, textures and caches must be packed into the file.</source>
        <translation>只发送 .blend 时，贴图和缓存必须打包进文件。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="335"/>
        <source>Blender Asset Tracer</source>
        <translation>Blender Asset Tracer</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="336"/>
        <source>BAT location</source>
        <translation>BAT 位置</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="337"/>
        <source>Directory that contains the blender_asset_tracer package.</source>
        <translation>包含 blender_asset_tracer 包的目录。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="341"/>
        <location filename="../../src/ui/pages/settings_page.cpp" line="343"/>
        <source>Worker cache</source>
        <translation>工作缓存</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="344"/>
        <source>Where projects from other nodes are kept while this machine renders them.</source>
        <translation>本机为其他节点渲染时存放其工程文件的位置。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="405"/>
        <source>Hide</source>
        <translation>隐藏</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="413"/>
        <source>New farm key</source>
        <translation>新集群密钥</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="414"/>
        <source>Machines that keep the old key can no longer work with this one. Generate a new key?</source>
        <translation>仍用旧密钥的机器将无法和本机协作。确定生成新密钥？</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="597"/>
        <source>Checking Blender...</source>
        <translation>正在检查 Blender...</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="681"/>
        <source>Reset every setting to its default? The farm key, manual nodes, Blender versions and language are kept.</source>
        <translation>将所有设置恢复为默认值？集群密钥、手动节点、Blender 版本和语言会保留。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="765"/>
        <source>The farm key needs at least %1 characters. Not saved yet.</source>
        <translation>集群密钥至少需要 %1 个字符，尚未保存。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="549"/>
        <source>No Blender added yet.</source>
        <translation>尚未添加 Blender。</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="570"/>
        <source>Remove</source>
        <translation>移除</translation>
    </message>
    <message>
        <location filename="../../src/ui/pages/settings_page.cpp" line="772"/>
        <source>Failed to save distributed.json.</source>
        <translation>保存 distributed.json 失败。</translation>
    </message>
</context>
<context>
    <name>Widget</name>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="135"/>
        <source>Minimize</source>
        <translation>最小化</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="119"/>
        <location filename="../../src/ui/window/widget.cpp" line="136"/>
        <source>Maximize</source>
        <translation>最大化</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="119"/>
        <source>Restore</source>
        <translation>还原</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="137"/>
        <source>Close</source>
        <translation>关闭</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="262"/>
        <location filename="../../src/ui/window/widget.cpp" line="470"/>
        <location filename="../../src/ui/window/widget.cpp" line="560"/>
        <source>Warning</source>
        <translation>警告</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="260"/>
        <source>Some files are still rendering. Are you sure to exit?</source>
        <translation>一些文件仍在渲染中，确定要退出吗？</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="176"/>
        <source>Rendrop keeps running in the tray. Right-click the icon to quit.</source>
        <translation>Rendrop 仍在托盘中运行。右键单击图标可退出。</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="211"/>
        <source>Show Rendrop</source>
        <translation>显示 Rendrop</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="213"/>
        <source>Quit</source>
        <translation>退出</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="261"/>
        <source>Some files are still loading. Are you sure to exit?</source>
        <translation>一些文件仍在加载中，确定要退出吗？</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="385"/>
        <source>Queue</source>
        <translation>队列</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="386"/>
        <source>Nodes</source>
        <translation>节点</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="387"/>
        <source>Settings</source>
        <translation>设置</translation>
    </message>
    <message numerus="yes">
        <location filename="../../src/ui/window/widget.cpp" line="435"/>
        <source>Rendering · %n node(s)</source>
        <translation>
            <numerusform>渲染中 · %n 个节点</numerusform>
        </translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="435"/>
        <source>Rendering</source>
        <translation>渲染中</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="441"/>
        <source>No Blender</source>
        <translation>无 Blender</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="448"/>
        <source>Ready</source>
        <translation>就绪</translation>
    </message>
    <message numerus="yes">
        <location filename="../../src/ui/window/widget.cpp" line="452"/>
        <source>%1 · %n peer(s)</source>
        <translation>
            <numerusform>%1 · %n 个节点</numerusform>
        </translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="491"/>
        <source>Select Blender Version</source>
        <translation>选择 Blender 版本</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="493"/>
        <source>Blender Executable (blender.exe)</source>
        <translation>Blender 主程序 (blender.exe)</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="501"/>
        <location filename="../../src/ui/window/widget.cpp" line="526"/>
        <location filename="../../src/ui/window/widget.cpp" line="534"/>
        <location filename="../../src/ui/window/widget.cpp" line="570"/>
        <source>Error</source>
        <translation>错误</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="501"/>
        <source>Cannot open specified path.</source>
        <translation>打开指定的路径时出现错误.</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="526"/>
        <source>Failed to start Blender.</source>
        <translation>Blender 启动失败.</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="530"/>
        <source>Note</source>
        <translation>提示</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="530"/>
        <source>Selected Blender version already exists.</source>
        <translation>选择的 Blender 版本已存在.</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="534"/>
        <location filename="../../src/ui/window/widget.cpp" line="570"/>
        <source>Failed to save version configuration file.</source>
        <translation>保存配置文件时出现错误.</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="561"/>
        <source>Remove Blender %1 from Rendrop? The program itself is not deleted.</source>
        <translation>从 Rendrop 中移除 Blender %1？程序本身不会被删除。</translation>
    </message>
    <message>
        <location filename="../../src/ui/window/widget.cpp" line="471"/>
        <source>Language settings will take effect after restarting the program. Restart now?</source>
        <translation>语言设置将在重启程序后生效，是否重启程序？</translation>
    </message>
</context>
<context>
    <name>fileBar</name>
    <message>
        <location filename="../../src/ui/forms/file_bar.ui" line="26"/>
        <source>Form</source>
        <translation>Form</translation>
    </message>
    <message>
        <location filename="../../src/ui/forms/file_bar.ui" line="126"/>
        <source>File</source>
        <translation>文件</translation>
    </message>
    <message>
        <location filename="../../src/ui/forms/file_bar.ui" line="151"/>
        <location filename="../../src/ui/forms/file_bar.ui" line="176"/>
        <location filename="../../src/ui/forms/file_bar.ui" line="210"/>
        <source>...</source>
        <translation>...</translation>
    </message>
    <message>
        <location filename="../../src/ui/forms/file_bar.ui" line="268"/>
        <source>0/1000 (0%)</source>
        <translation>0/1000 (0%)</translation>
    </message>
</context>
<context>
    <name>queuePage</name>
    <message>
        <location filename="../../src/ui/forms/queue_page.ui" line="87"/>
        <source>Blender version used for rendering</source>
        <translation>用于渲染的 Blender 版本</translation>
    </message>
    <message>
        <location filename="../../src/ui/forms/queue_page.ui" line="164"/>
        <source>Start Render</source>
        <translation>开始渲染</translation>
    </message>
    <message>
        <location filename="../../src/ui/forms/queue_page.ui" line="306"/>
        <source>Output</source>
        <translation>输出</translation>
    </message>
    <message>
        <location filename="../../src/ui/forms/queue_page.ui" line="362"/>
        <source>Show log</source>
        <translation>显示日志</translation>
    </message>
</context>
<context>
    <name>widget</name>
    <message>
        <location filename="../../src/ui/forms/widget.ui" line="26"/>
        <location filename="../../src/ui/forms/widget.ui" line="111"/>
        <source>Rendrop</source>
        <translation>Rendrop</translation>
    </message>
</context>
</TS>
