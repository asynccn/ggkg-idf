# Resources Directory

This directory contains resources used by the project, mainly payloads that are sent using HTTP.

Files in this directory are not used by the project directly. All files in this directory will be compressed to GZIP format and turned into C char arrays in `main/htdocs.h`.

To compress and turn into C char arrays, turn to [Encoding](https://gchq.github.io/CyberChef/#recipe=Gzip('Dynamic%20Huffman%20Coding','index_ov2640.html','',true)To_Hex('0x%20with%20comma',0)); for decompressing procedure turn to [Decoding](https://gchq.github.io/CyberChef/#recipe=Remove_whitespace(true,true,true,true,true,false)Find_/_Replace(%7B'option':'Regex','string':','%7D,'',true,false,true,false)From_Hex('0x')Gunzip()).
