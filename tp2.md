> # Steps
> 
> Reset dos PCs:
> 
> Terminal: systemctl restart networking
> 
> Ligar Cisco -> Rs232 ao console do switch
>
> ## IP Network
>
> 1. Conectar E0 do Tux53 à porta 1 e do Tux54 à porta 2 do switch;
> 2. Aceder a cada um dos computadores e configurar os seus IPs:
> 
>     ```bash
>     $ ifconfig eth0 up
>     $ ifconfig eth0 <IP>
>         - 172.16.50.1/24 para o Tux53
>         - 172.16.50.254/24 para o Tux54
>     ```
> 
> 3. Para ver o MAC address de cada computador, consultar o campo `ether` no comando `ipconfig`;
> 4. Verificar a conexão dos dois computadores na mesma rede. Em cada um tenta enviar pacotes de dados para outro. A conexão está correcta quando todos os pacotes são recebidos:
> 
>     ```bash
>     $ ping <IP> -c 20
>         - 172.16.50.254 para o Tux53
>         - 172.16.50.1 para o Tux54
>     ```
> 
> 5. Avaliar a tabela *Address Resolution Protocol* (ARP) do Tux53. Deverá ter uma única entrada com o IP e o MAC do Tux54. Para visualizar a tabela:
> 
>     ```bash
>     $ arp -a # ?(172.16.50.254) at 00:21:5a:c3:78:70 [ether] on eth0
>     ```
> 
> 6. Apagar a entrada da tabela ARP do Tux53:
> 
>     ```bash
>     $ arp -d 172.16.50.254/24
>     $ arp -a # empty
>     ```
>
> ## 2 bridges
> 1. Conectar Tux52_S0 em Rs232 –> Cisco
> 3. Abrir o GKTerm no Tux52 e configurar a baudrate para 115200;
> 4. Resetar as configurações do switch com o seguinte comando:
> 
>     ```bash
>     > admin
>     > /system reset-configuration
>     > y
>     ```
> 
> 5. Conectar o Tux52_E0 ao switch (porta 3) e configurar a ligaçao com os seguintes comandos:
> 
>     ```bash
>     $ ifconfig eth0 up
>     $ ifconfig eth0 172.16.51.1/24
>     ```
> 
> 6. Criar 2 bridges no switch
> 
>     ```bash
>     > /interface bridge add name=bridge50
>     > /interface bridge add name=bridge51
>     ```
> 
> 7. Eliminar as portas as quais o Tux52, 53 e 54 estao ligados por defeito
> 
>     ```bash
>     > /interface bridge port remove [find interface=ether1] 
>     > /interface bridge port remove [find interface=ether2] 
>     > /interface bridge port remove [find interface=ether3] 
>     ```
> 
> 8. Adicionar as novas portas
> 
>     ```bash
>     > /interface bridge port add bridge=bridge50 interface=ether1
>     > /interface bridge port add bridge=bridge50 interface=ether2 
>     > /interface bridge port add bridge=bridge51 interface=ether3
>     ```
> 
> 9. Verificar que as portas foram adicionadas corretamente com o commando :
> 
>     ```bash
>     > /interface bridge port print
>     ```
> ## Router in Linux
> 
> 1. Ligar eth1 do Tux54 à porta 4 do switch. Configurar eth1 do Tux54
> ```bash
>     ifconfig eth1 up
>     ifconfig eth1 172.16.51.253/24
> ```
> 
> 2. Eliminar as portas as quais o Tux52 esta ligado por defeito e adicionar a nova porta
> ```bash
>     /interface bridge port remove [find interface=ether4]
>     /interface bridge port add bridge=bridge51 interface=ether4
> ```
> 
> 3. Ativar *ip forwarding* e desativar ICMP no Tux54
> ```bash
>     #3 Ip forwarding t4
>     sysctl net.ipv4.ip_forward=1
> 
>     #4 Disable ICMP echo ignore broadcast T4
>     sysctl net.ipv4.icmp_echo_ignore_broadcasts=0
> ```
> 
> 4. Observar MAC e Ip no Tux54eth0 e Tux54eth1
> ```bash
>     ifconfig
>     ##Mac eth0 -> 00:21:5a:c3:78:70
>     ##ip eth0-> 172.16.50.254
>     ##Mac eth1 -> 00:c0:df:08:d5:b0
>     ##ip eth1-> 172.16.51.253
> ```
> 
> 5. Adicionar as seguintes rotas no Tux52 e Tux53 para que estes se consigam altançar um ou outro através do Tux54
> ```bash
>     route add -net  172.16.50.0/24 gw 172.16.51.253 # no Tux52
>     route add -net  172.16.51.0/24 gw 172.16.50.254 # no Tux53
> ```
> 
> 6. Verificar as rotas em todos os Tux com o seguinte comando:
> ```bash
>     route -n
> ```
>
> ## Commercial Router and NAT
>
> 1. Ligar eth1 do Router à porta 5.1 da régua
>
> 2. Ligar eth2 do Router ao Switch porta 5
> 
> 3. Eliminar as portas default do ether5 do switch e ligar o ether5 à bridge51
> 
> ```bash
>     /interface bridge port remove [find interface=ether5]
>     /interface bridge port add bridge=bridge51 interface=ether5
> ```
> 
> 4. Trocar o cabo ligado à consola do Switch para o Router MTIK
> 
> 5. No Tux52 conectar ao router desde o GTKterm com:
> 
> ```note
>     Serial Port: /dev/ttyS0
>     Baudrate: 115200
>     Username: admin
>     Password: <ENTER>
> ```
> 
> 6. Resetar as configuraçoes do router
> 
> ```bash
>     /system reset-configuration
> ```
> 
> 7. Configurar ip do Router pela consola do router no GTKterm do Tux52
> 
> ```bash
>     /ip address add address=172.16.2.59/24 interface=ether1
>     /ip address add address=172.16.51.254/24 interface=ether2
> ```
> 
> 8. Configurar as rotas default nos Tuxs e no Router:
> 
> ```bash
>     route add default gw 172.16.51.254 # Tux52
>     route add default gw 172.16.50.254 # Tux53
>     route add default gw 172.16.51.254 # Tux54
> 
>     /ip route add dst-address=172.16.50.0/24 gateway=172.16.51.253  # Router console
>     /ip route add dst-address=0.0.0.0/0 gateway=172.16.2.254        # Router console
> ```
> 
> ## DNS
> 1. Em todos os Tuxs, adicionar a seguinte linha ao ficheiro /etc/resolv.conf
> 
> ```note
> nameserver 172.16.2.1
> ```
> 2. Em todos os Tux, fazer ping do google para verificar se podem ser usados nomes como host
> 
> ```bash
> ping google.com
> ```
> 
> ## TCP Connections
> 1. No Tux53, fazer o download de um ficheiro do servidor FTP. Guardar a captura obtida com o WireShark.
> 2. No Tux53 e no Tux52, fazer o download de um ficheiro do servidor FTP ao mesmo tempo. Guardar a captura obtida com o WireShark.
