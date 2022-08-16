# -*- coding: UTF-8 -*-
import time
import pdb
import socket
from fabric import Connection
import signal
import os


TEST_USER = 'vagrant'
TEST_PASS = 'vagrant'
EXITING = False

# 重置网络环境
def reset_network(conn):
    reset_networf_cmd = 'sudo tcset enp0s8 --rate 100Mbps --delay 20ms --overwrite'
    rst = conn.run(reset_networf_cmd)
    if (rst.failed):
        print("[自动测试] 重置服务端网络失败") 
        exit()   
    rst = conn.local(reset_networf_cmd)
    if (rst.failed):
        print("[自动测试] 重置客户端网络失败")
        exit()  

# 关闭正在运行的server和client进程 
def close_server_and_client(conn):
    stop_server_cmd = 'sudo tmux kill-session -t pytest_server'
    stop_client_cmd = 'sudo tmux kill-session -t pytest_client'
    try:
        conn.local('sudo tmux has-session -t pytest_client', hide=True)
        rst = conn.local(stop_client_cmd)
        if (rst.failed):
            print("[自动测试] 关闭本地测试程序失败")
    except Exception as e:
        pass 
    try:
        conn.run('sudo tmux has-session -t pytest_server', hide=True)
        rst = conn.run(stop_server_cmd)
        if (rst.failed):
            print("[自动测试] 关闭服务端测试程序失败")
    except Exception as e: 
        pass 

# 根据提供的指令运行服务端和客户端
def run_server_and_client(conn, start_server_cmd, start_client_cmd):
    rst = conn.run(start_server_cmd)
    if (rst.failed):
        print("[自动测试] 无法运行服务端测试程序")
        exit()
    rst = conn.local(start_client_cmd)
    if (rst.failed):
        print("[自动测试] 无法运行客户端测试程序")
        exit()    

# 响应用户ctrl c 防止server client正在运行过程中退出测试程序而没有关闭响应进程
def my_exit(signum, frame):
    global EXITING
    if EXITING==True:
        print('\n正在退出测试程序, 请稍等, 不需要多次停止\n')
        return
        
    EXITING = True
    print('\n正在退出测试程序, 请稍等\n')
    with Connection(host="10.0.0.1", user=TEST_USER, connect_kwargs={'password':TEST_PASS}) as conn:
        reset_network(conn)
        close_server_and_client(conn)
    exit()

# 编译所有源码 包括项目源码和测试源码
def compile_source_files(conn):
    print("[自动测试] 编译提交源码")
    print("> cd /vagrant/tju_tcp && make")
    rst = conn.run("cd /vagrant/tju_tcp && make", timeout=10)
    if (rst.failed):
        print('[自动测试] 编译提交源码错误 停止测试')
        print('{"scores": {"establish_connection": 0}}')
        exit()
    print("")
    
    # 判断是否使用用户提交的新Makefile
    if os.path.exists("/vagrant/tju_tcp/test_Makefile"):
        print("[自动测试] 检测到用户提供了自己的测试Makefile, 使用用户的测试Makefile替换自带的版本")
        print("> cp /vagrant/tju_tcp/test_Makefile /vagrant/tju_tcp/test/Makefile")
        rst = conn.local("cp /vagrant/tju_tcp/test_Makefile /vagrant/tju_tcp/test/Makefile", timeout=10)
        if (rst.failed):
            print("[自动测试] 移动测试Makefile失败")
            exit()
    print("")

    print("[自动测试] 编译测试源码")
    print("> cd /vagrant/tju_tcp/test && make")
    rst = conn.run("cd /vagrant/tju_tcp/test && make", timeout=10)
    if (rst.failed):
        print('[自动测试] 编译测试源码错误 停止测试')
        print('{"scores": {"establish_connection": 0}}')
        exit()    
    print("")

def test_establish_connection(conn):
    print("============================ 建立连接的测试 ================================")
    # 首先要保证网络没问题
    reset_network(conn)


    print("[建立连接测试] 开启服务端和客户端 将输出重定向到文件")
    start_server_cmd = 'sudo tmux new -s pytest_server -d "sudo bash -c /vagrant/tju_tcp/test/receiver > /vagrant/tju_tcp/test/server.log 2>&1"'
    start_client_cmd = 'sudo tmux new -s pytest_client -d "sudo bash -c /vagrant/tju_tcp/test/test_client > /vagrant/tju_tcp/test/client.log 2>&1"'
    run_server_and_client(conn, start_server_cmd, start_client_cmd)
    print("")


    print("[建立连接测试] 等待8s测试结束 三次握手应该在8s内完成")
    conn.run('sleep 8')
    print("")

    # 关闭双端
    close_server_and_client(conn)
    

    print("[建立连接测试] 打印文件里面的日志")
    cat_log_server_cmd = 'sudo cat /vagrant/tju_tcp/test/server.log'
    cat_log_client_cmd = 'sudo cat /vagrant/tju_tcp/test/client.log' 
    print("===================================================")
    server_log = conn.run(cat_log_server_cmd)
    client_log = conn.local(cat_log_client_cmd)
    print("===================================================")
    print("")

    print("[建立连接测试] 进行评分 正确发送第一次握手SYN数据包 +50分 能够建立连接 +50分")
    if "{{TEST SUCCESS}}" in server_log.stdout:
        estabcon_score = 100
    elif "{{TEST FAILED}}" in server_log.stdout:
        score_count = server_log.stdout.count("{{GET SCORE}}")
        estabcon_score = score_count*50
    else:
        print('[建立连接测试] 测试结果解析出错')
        estabcon_score = 0 

    print("")
    print("[建立连接测试] 建立连接部分的得分为 %d 分"%estabcon_score)

    print("===========================================================================")
    print("===========================================================================")

    return estabcon_score

def test_reliable_data_transfer(conn):
    print("========================== 可靠数据传输的测试 =============================")
    # 首先要保证网络没问题
    reset_network(conn)

    print("[数据传输测试] 开启服务端和客户端 等待5s建立连接")
    start_server_cmd = 'sudo tmux new -s pytest_server -d "sudo bash -c /vagrant/tju_tcp/test/rdt_server > /vagrant/tju_tcp/test/server.log 2>&1"'
    start_client_cmd = 'sudo tmux new -s pytest_client -d "sudo bash -c /vagrant/tju_tcp/test/rdt_client > /vagrant/tju_tcp/test/client.log 2>&1"'
    run_server_and_client(conn, start_server_cmd, start_client_cmd)

    time.sleep(5)
    print("")

    print("[数据传输测试] 设置双方的网络通讯速率 丢包率 和延迟")
    set_networf_cmd = 'sudo tcset enp0s8 --rate 100Mbps --delay 300ms --loss 10% --overwrite'
    rst = conn.run(set_networf_cmd)
    if (rst.failed):
        print("[建立连接测试] 无法设置服务端网络")
        exit()
    rst = conn.local(set_networf_cmd)
    if (rst.failed):
        print("[建立连接测试] 无法设置客户端网络")
        exit()
    print("")

    print("[数据传输测试] 等待40s进行数据传输")
    time.sleep(40)
    print("")

    print("[数据传输测试] 计时器到时 关闭双端")
    # 关闭双端
    close_server_and_client(conn)
    print("")

    # 取消网络设置
    reset_network(conn)

    print("[数据传输测试] 检查数据传输是否完整")
    cat_log_server_cmd = 'sudo cat /vagrant/tju_tcp/test/server.log'
    cat_log_client_cmd = 'sudo cat /vagrant/tju_tcp/test/client.log' 
    print("===================================================")
    print("===================server 日志=====================")
    server_log = conn.run(cat_log_server_cmd)
    print("===================client 日志=====================")
    client_log = conn.local(cat_log_client_cmd)
    print("===================================================")
    print("")


    print("[数据传输测试] 进行评分 共发送50条数据 每成功接收一条得2分")
    rdt_score = 0
    for i in range(0,50):
        if "[RDT TEST] server recv test message%d"%i in server_log.stdout:
            rdt_score = rdt_score + 2
    print("")
    print("[数据传输测试] 可靠数据传输得分为 %d 分"%rdt_score)

    return rdt_score

def test_close_connection(conn):
    print("========================== 断开连接的测试 =============================")
    # 首先要保证网络没问题
    reset_network(conn)

    print("======= 测试双方先后关闭连接的情况 =======")

    print("[双方先后关闭测试] 开启服务端和客户端 将输出重定向到文件")
    start_server_cmd = 'sudo tmux new -s pytest_server -d "sudo bash -c /vagrant/tju_tcp/test/close_server 0 > /vagrant/tju_tcp/test/server.log 2>&1"'
    start_client_cmd = 'sudo tmux new -s pytest_client -d "sudo bash -c /vagrant/tju_tcp/test/close_client > /vagrant/tju_tcp/test/client.log 2>&1"'
    run_server_and_client(conn, start_server_cmd, start_client_cmd)
    print("")

    print("[双方先后关闭测试] 等待12s测试结束 三次握手以及四次挥手放在一起应该在12s内完成")
    conn.run('sleep 12')
    print("")

    close_server_and_client(conn)

    print("[双方先后关闭测试] 打印文件里面的日志")
    cat_log_server_cmd = 'sudo cat /vagrant/tju_tcp/test/server.log'
    cat_log_client_cmd = 'sudo cat /vagrant/tju_tcp/test/client.log' 
    print("===================================================")
    print("===================server 日志=====================")
    server_log = conn.run(cat_log_server_cmd)
    print("===================client 日志=====================")
    client_log = conn.local(cat_log_client_cmd)
    print("===================================================")
    print("")

    print("[双方先后关闭测试] 进行评分 ")
    normal_close_score = 0
    if "{{FIRST FIN PASSED TEST}}" in server_log.stdout:
        normal_close_score = normal_close_score + 30
    if "{{FINAL ACK PASSED TEST}}" in server_log.stdout:
        normal_close_score = normal_close_score + 30

    print("")
    print("[双方先后关闭测试] 双方先后关闭测试部分的得分为 %d 分 (满分60分)"%normal_close_score)


    print("")
    print("")
    print("======= 测试双方同时关闭连接的情况 =======")

    print("[双方同时关闭测试] 开启服务端和客户端 将输出重定向到文件")
    start_server_cmd = 'sudo tmux new -s pytest_server -d "sudo bash -c \'/vagrant/tju_tcp/test/close_server 1\' > /vagrant/tju_tcp/test/server.log 2>&1"'
    start_client_cmd = 'sudo tmux new -s pytest_client -d "sudo bash -c \'/vagrant/tju_tcp/test/close_client\' > /vagrant/tju_tcp/test/client.log 2>&1"'
    run_server_and_client(conn, start_server_cmd, start_client_cmd)
    print("")


    print("[双方同时关闭测试] 等待12s测试结束 三次握手以及四次挥手放在一起应该在12s内完成")
    conn.run('sleep 12')
    print("")

    # 后台静默关闭双端
    close_server_and_client(conn)
    

    print("[双方同时关闭测试] 打印文件里面的日志")
    cat_log_server_cmd = 'sudo cat /vagrant/tju_tcp/test/server.log'
    cat_log_client_cmd = 'sudo cat /vagrant/tju_tcp/test/client.log' 

    print("===================================================")
    print("===================server 日志=====================")
    server_log = conn.run(cat_log_server_cmd)
    print("===================client 日志=====================")
    client_log = conn.local(cat_log_client_cmd)
    print("===================================================")
    print("")

    print("[双方同时关闭测试] 进行评分 ")

    simul_close_score = 0
    if "{{FIRST FIN PASSED TEST}}" in server_log.stdout:
        simul_close_score = simul_close_score + 20
    if "{{FINAL ACK PASSED TEST}}" in server_log.stdout:
        simul_close_score = simul_close_score + 20

    print("")
    print("[双方同时关闭测试] 双方同时关闭测试部分的得分为 %d 分 (满分40分)"%simul_close_score)


    close_score = normal_close_score+simul_close_score
    print("[断开连接的测试] 两种情况的总得分为 %d 分 (满分100分)"%(close_score))

    return close_score


def main():
    if socket.gethostname() == "server":
        print("只能在client端运行自动测试")
        exit()

    signal.signal(signal.SIGINT, my_exit)
    signal.signal(signal.SIGTERM, my_exit)

    estabcon_score = 0
    rdt_score = 0
    close_score = 0
    print("====================================================================")
    print("============================开始测试================================")
    print("====================================================================")
    with Connection(host="10.0.0.1", user=TEST_USER, connect_kwargs={'password':TEST_PASS}) as conn:
        # 编译测试源码
        compile_source_files(conn)
        # 进行建立连接的测试
        estabcon_score = test_establish_connection(conn)
        print("")
        print("")
        # 进行可靠数据传输的测试
        rdt_score = test_reliable_data_transfer(conn)
        print("")
        print("")
        # 进行关闭连接的测试
        close_score = test_close_connection(conn)
        print("")
        print("")
        print("========================= 所有测试项目得分汇总 ============================")
        print('{"scores": {"establish_connection": %s, "reliable_data_transfer":%s, "close_connection":%s}}'%(estabcon_score, rdt_score, close_score))
    
            
if __name__ == "__main__":
    main()