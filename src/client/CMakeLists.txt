# 定义编译目录
aux_source_directory(. CLIENT_LIST)

# 添加生成文件列表
add_executable(ChatClient ${CLIENT_LIST})

# 指定生成可执行文件需要依赖的库
target_link_libraries(ChatClient pthread)