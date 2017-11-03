## 一键创建新用户+搭建网站脚本
### 用法(需先安装ansible,ansible-playbook):
#### 1. 重新生成hash密码, 用修改后的密码+用户名替换 webserver/defaults/main.yml 中的password
#### 2. (可选)为了直接ssh到新用户机器上，使用 ssh-keygen -t rsa -b 2048 生成 ~/.ssh/id_rsa.pub,并拷贝到 webserver/files/public_keys 中
#### 3. 更改域名为自己注册的域名
#### 4. 将网站所有文件拷贝到 www-files 目录下
#### 5. 修改 webserver/templates/gif5.j2,将80端口的return注释掉，运行如下命令部署web服务器
```
ansible-playbook -i ./hosts -v --ask-pass site.yml
-v代表输出调试日志,-vvvv可输出更详细的调试日志
```
#### 6. 此时的web服务器只能使用80端口,下载并使用 LetsEncrypt 的脚本为自己的域名生成 pem 文件
#### 7. 取消注释 80端口段的 return语句。重启nginx,即可开启全站https :-)
