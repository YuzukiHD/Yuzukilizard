演示一个recorder开启两个muxerChannel。一个muxerChannel将编码码流封装为mp4文件写入sd，一个muxerChannel为rawMuxer，
通过callback将编码数据送给recorder，再传给app。
支持多视频流封装情况下的两路MuxerChannel。callbackOut数据可以通过码流编号，选择需要的视频流和音频流。
本demo选择视频子码流streamId=1作为callbackOut的视频数据。

脚本test_loop.sh用于循环测试。
