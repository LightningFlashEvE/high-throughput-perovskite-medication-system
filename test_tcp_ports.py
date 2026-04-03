import socket
import sys
import time

TARGETS = [
    ("192.168.5.201", 4196, "tcpCore"),
    ("192.168.5.201", 4197, "tcpBalance"),
]

for ip, port, name in TARGETS:
    print(f"\n{'='*50}")
    print(f"测试 {name} -> {ip}:{port}")
    print(f"{'='*50}")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5)
    try:
        ret = s.connect_ex((ip, port))
        if ret == 0:
            print(f"[OK] 端口 {port} 可达")

            # 尝试接收banner或任何数据
            s.setblocking(False)
            time.sleep(1)
            try:
                data = s.recv(4096)
                if data:
                    print(f"[DATA] 收到数据: {data!r}")
                else:
                    print(f"[INFO] 无即时数据（可能需要发送查询命令）")
            except BlockingIOError:
                print(f"[INFO] 无即时数据（端口开放但无响应数据）")
        else:
            print(f"[FAIL] connect_ex 返回 {ret}")
            # 根据 errno 给提示
            if ret == 11:  # EAGAIN
                print("  -> 资源暂时不可用（高并发或队列满）")
            elif ret == 113:  # EHOSTUNREACH
                print("  -> 主机不可达（网络路由问题）")
            elif ret == 110:  # ETIMEDOUT
                print("  -> 连接超时（链路不通或被丢弃）")
            else:
                print(f"  -> errno={ret}")
    except socket.timeout:
        print(f"[FAIL] 连接超时（5s）")
    except socket.error as e:
        print(f"[FAIL] Socket错误: {e}")
    finally:
        s.close()

print(f"\n{'='*50}")
print("测试完成")
