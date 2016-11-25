# todo, ~/.bashrc ~/.bash_profile ~/.vimrc ~/.gdbinit 等文件也应该用同一份
# 确认在 Dockerfile 相同目录下的ssh_info 目录里面使用命令 ssh-keygen -q -t rsa -N '' -f ./id_rsa,然后将key 添加到git服务器授权key列表中
FROM ubuntu:latest
MAINTAINER Giant
# Avoid ERROR: invoke-rc.d: policy-rc.d denied execution of start.
RUN echo "#!/bin/sh\nexit 0" > /usr/sbin/policy-rc.d
RUN echo 'root:ztgame@123' | chpasswd
RUN apt-get update
# install ifconfig command
RUN apt-get install net-tools -y
# 先后顺序不能变,因为安装 libc6-dev 会卸载 gcc g++
RUN apt-get install libc6-dev -y
# In ubuntu server ,you should install libc6-dev-amd64, ubuntu server 上尽量安装 libc6-dev-amd64,这样只安装64位头文件
#RUN apt-get install libc6-dev-amd64 -y
RUN ln -s /usr/include/x86_64-linux-gnu/sys/ /usr/include/sys
RUN ln -s /usr/include/x86_64-linux-gnu/gnu/ /usr/include/gnu
RUN ln -s /usr/include/x86_64-linux-gnu/bits/ /usr/include/bits
# lbzip2 是为了tar -xf安装的
RUN apt-get install make gcc g++ gdb libmysql++-dev git vim lbzip2 -y
# 防止中文乱码
RUN locale-gen en_US.UTF-8
ENV LANG='en_US.UTF-8' LANGUAGE='en_US:en' LC_ALL='en_US.UTF-8'
# add user myuser and git clone mygit
RUN useradd -ms /bin/bash myuser
RUN echo myuser:myuser | /usr/sbin/chpasswd
USER myuser
WORKDIR /home/myuser
RUN echo "export KBE_CONFIG=\"Debug64\"" >> .bashrc
RUN mkdir .ssh
RUN ssh-keyscan 192.168.96.241 >> .ssh/known_hosts
ADD ssh_info/ .ssh/
USER root
RUN chmod 600 .ssh/id_rsa
RUN chmod 644 .ssh/id_rsa.pub
RUN chown myuser .ssh/*
RUN chgrp myuser .ssh/*
USER myuser
# Avoid first connection host confirmation, if pot is not 22
# RUN ssh-keyscan -p2200 your.git.server.tld > /root/.ssh/known_hosts
RUN git clone git@192.168.96.241:mygit.git
USER root
# 摘自官方ssh示例,尝试过自己写,但是没卵用
RUN apt-get install -y openssh-server
RUN mkdir -p /var/run/sshd
RUN sed -i 's/PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config
# SSH login fix. Otherwise user is kicked off after login
RUN sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd
ENV NOTVISIBLE "in users profile"
RUN echo "export VISIBLE=now" >> /etc/profile

CMD ["/usr/sbin/sshd", "-D"]
