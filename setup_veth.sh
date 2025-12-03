 #!/bin/bash

 # ================= 配置区域 =================

 # 1. 你的协议栈 IP (必须与 include/config.h 中的 NET_IF_IP 保持一致)
 #    建议使用 192.168.xxx.xxx 等私有网段，避免与物理网络 (如 10.x.x.x) 冲突
 #    默认: 192.168.254.2
 STACK_IP="192.168.254.2"

 # 2. 网络前缀 (用于生成相关 IP)
 #    默认: 192.168.254
 NET_PREFIX="192.168.254"

 # 3. 接口名称
 VETH_USER="veth0"    # 绑定给用户态协议栈的接口
 VETH_KERNEL="veth1"  # 内核/客户端使用的接口

 # ===========================================

 # 自动生成其他配置
 # veth0 的 IP (必须与 STACK_IP 不同，但同网段，用于 driver_find 发现接口)
 # 注意：我们不能给 veth0 配置 $STACK_IP，因为 driver_find 会拒绝与本机 IP 完全相同的接口
 VETH_USER_IP="${NET_PREFIX}.3/24"

 # veth1 的 IP (作为客户端/网关 IP)
 VETH_KERNEL_IP="${NET_PREFIX}.1/24"

 # 路由网段
 SUBNET="${NET_PREFIX}.0/24"

 echo "=== Setting up veth pair for Net-Lab ==="
 echo "Configuration:"
 echo "  Stack IP:   $STACK_IP"
 echo "  Prefix:     $NET_PREFIX"
 echo "  Interfaces: $VETH_USER <-> $VETH_KERNEL"

 # 1. 清理旧接口
 if ip link show $VETH_USER > /dev/null 2>&1; then
     echo "Removing existing $VETH_USER..."
     sudo ip link del $VETH_USER
 fi

 # 2. 创建 veth pair
 echo "Creating veth pair: $VETH_USER <-> $VETH_KERNEL"
 sudo ip link add $VETH_USER type veth peer name $VETH_KERNEL

 # 3. 启动接口
 echo "Bringing up interfaces..."
 sudo ip link set $VETH_USER up
 sudo ip link set $VETH_KERNEL up

 # 4. 配置 IP
 # 给 veth0 配置一个同网段的 IP，以便 driver_find 能找到它 (它查找最长前缀匹配)
 echo "Assigning IP $VETH_USER_IP to $VETH_USER"
 sudo ip addr add $VETH_USER_IP dev $VETH_USER

 # 给 veth1 配置网关 IP
 echo "Assigning IP $VETH_KERNEL_IP to $VETH_KERNEL"
 sudo ip addr add $VETH_KERNEL_IP dev $VETH_KERNEL

 # 5. 路由调整
 # 因为 veth0 和 veth1 都在同一个网段，内核会自动添加两条路由。
 # 我们希望内核发往 $SUBNET 的包走 veth1 (这样才能通过 veth pair 到达 veth0 被我们捕获)
 # 所以我们要删除 veth0 的自动路由
 echo "Deleting kernel route for $VETH_USER..."
 sudo ip route del $SUBNET dev $VETH_USER

 # 6. 关闭 Checksum Offload (关键步骤)
 echo "Disabling checksum offload..."
 sudo ethtool -K $VETH_USER rx off tx off
 sudo ethtool -K $VETH_KERNEL rx off tx off

 # 7. 配置 IPv6 (双栈测试需要)
 echo "Configuring IPv6..."
 # 启用 IPv6
 sudo sysctl -w net.ipv6.conf.$VETH_USER.disable_ipv6=0 > /dev/null
 sudo sysctl -w net.ipv6.conf.$VETH_KERNEL.disable_ipv6=0 > /dev/null

 # 配置 veth1 的 IPv6 地址 (作为网关/测试端)
 # 对应 net_if_ip6 = fe80::1 (Link Local) 或者我们可以配一个 Global/ULA
 # 这里我们配置一个 ULA 地址 fd00::1/64
 VETH_KERNEL_IP6="fd00::1/64"
 echo "Assigning IPv6 $VETH_KERNEL_IP6 to $VETH_KERNEL"
 sudo ip -6 addr add $VETH_KERNEL_IP6 dev $VETH_KERNEL

 # 注意：veth0 不需要配置 IPv6 地址，因为我们的协议栈会直接处理发往 fd00::2 的包
 # 但是为了方便 ping6 fd00::2 能通，我们需要在 veth1 上添加邻居表项(NDP)或者路由
 # 如果实现了 NDP，则不需要手动添加。如果没实现 NDP，需要:
 # echo "Adding static NDP entry for fd00::2..."
 # sudo ip -6 neigh replace fd00::2 lladdr 00:11:22:33:44:55 dev $VETH_KERNEL

 # 8. 防止内核在 veth0 上响应 ARP (因为用户态协议栈要处理)
 echo "Disabling ARP on $VETH_USER..."
 sudo ip link set dev $VETH_USER arp off

 echo "=== Setup Complete ==="
 echo "User Stack Interface: $VETH_USER (Bind your tcp_server here)"
 echo "Kernel Interface:     $VETH_KERNEL (IP: $VETH_KERNEL_IP)"
 echo "Test Command:         nc $STACK_IP 60000"
 echo "Web Test:             curl http://$STACK_IP/"