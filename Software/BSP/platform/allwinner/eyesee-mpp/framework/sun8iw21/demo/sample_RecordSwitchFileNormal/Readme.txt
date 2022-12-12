两路录制。
主码流定时切换文件。
app收到主码流切换文件完成的消息后，主动调用子码流Recorder的switchFileNormal()进行文件切换。
