REM 更新客户端/服务器代码,更新客户端代码前先回滚修改
REM Unity中需设置 Editor --> Preferences --> Load previous project on startup,这样就可以直接启动客户端工程,不需要选择

REM 杀掉unity
tasklist | find /i "unity.exe" && taskkill /im unity.exe /F || echo process "unity.exe" not running.

svn cleanup client
svn revert -R client
svn up client
svn cleanup server
svn up server
REM 后台启动unity进程,网上查了下unity以后台运行时,无法获得显示设备。这里直接--batchmode以后台运行,来编译脚本和assets
REM START /B "my unity" "C:\Program Files\Unity\Editor\Unity.exe"  --projectPath "D:\dev\twleve\client\trunk\Valkyrie"
START /B "my unity" "C:\Program Files\Unity\Editor\Unity.exe" --batchmode --projectPath "D:\dev\twleve\client\trunk\Valkyrie"
REM set /p DUMMY=Hit ENTER to continue...