## Still a lot to be learnt
### 旧版服务器总结

#### 单区Server

1. **Super** 消息转发到平台，与其他区互通消息.主要连接 FLServer登陆, Coordinate协调, InfoClient GM工具服务器, Monitor监控服务器, RoleregServer名字编码重复验证, SNSServer嘟嘟服务器, SoundServer语音服务器
2. **Bill** 计费服务器,跟玩家钱相关的操作都在BillServer完成,关键类 BillUser,BillClient(与平台直连) 关键函数 bool action(int actionid),根据actionid对BillUser做对应处理.Bill上收到平台的充值消息,生成一个临时的序列号,将该tid发给平台;等平台返回消息时候再验证tid,并做了超时交易无效的机制.
3. **Record** 读档,写档(主要是CHARBASE),网关登陆时,场景写档,会话更新官职等信息时候会发消息record让record写档.
4. **Session** 主要用来管理唯一性全局的数据,比如家族帮会国家,国家战力,摆摊,邮件,摆摊,庄园,运镖,国家活动等;副本ID分配器,组队,交易,交易等图形验证.
5. **Scene** 不连接数据库,与玩家交互最频繁的服务器,玩家移动PK,道具包裹,装备,技能,任务,篝火,坐骑马匹,宠物系统。每个其他服务器全局管理的类,基本都对应场景上一个直接处理的类,比如交易,组队,运镖.最大SceneUser类,最频繁 setupCharbase(), sendMeToNine(), 道具Object类.
6. **Gateway** 客户端和服务器交互的通道,消息分发器,最主要是维护了九屏,每次要发9屏,25屏,正向5屏,反向5屏,都是网关直接发.
7. **Common** 一个服务器框架,定义了基本的线程池,有 superClient, recordClient, sessionClient, TimeTick
8. **DM** 数据管理服务器,管理全局数据,如排行榜,雄霸江湖擂台赛,各种PK赛,秘境寻宝
9. **Function** 功能服务器,管理全局数据,封印之地,拍卖行,以武会友,跑商,彩票,求购管理器等.
10. **Community** 社区服务器,管理全局唯一数据,比如等级竞速,互动推送.
11. **RelationServer** 好友黑名单社会关系,成就,每日签到,(CommunityServer, Function, DMServer, RelationServer 都是继承 CommonServer的,其实可以合在一起的)
12. **MiniServer** 小游戏服务器,主要是玩骰子,斗地主等小游戏用的,这个服务器其实没有存在的必要,直接session写个管理器就行了.
13. **NpcServer** NPC剧场,NPC队伍,NPC AI,寻路等判断.
14. **BlogServer** 鸡肋的服务器,可以删了,处理拉人，心情等,完全可以放session上,session也有拉人的


#### 全区 Server  
1. **CoordinateServer** 跨区功能服务器,跨区天地,跨区侠义岛,试练战,职业赛等,每次都要同步一份角色的数据到 Coordinate. 
2. **AppServer** 游戏与网页交互的通道,HttpTask 处理json请求, CharTask 返回 json字符串.


### 主要服务器线程

所有都需要启动的线程 :

1. 主线程,while 循环调用 accept接受新的客户端
2. TimeTick 线程,不停地loop，遍历处理消息,定时触发时间.
3. TCP连接线程池,最基本的保证4个线程, verify线程,sync线程,ok线程,recycle线程,但okay和verify线程是动态扩张的,一直可以扩张到 `num = 连接上限 / 每个线程单独处理的连接数` 个线程
4. 所有非 Super 以外的服务器,都会单独启动一个 superClient 用于和super之间通信.
 
每个服务器单独启动的线程:

1. Super, 每连接一个平台就要单独启动个线程作为 client, 不断去调用 recv接收消息.比如登陆服务器,名字验证,协调服务器,网页交互服务器,IM服务器等..除此之外,不单独启动线程.
2. Record, 没啥特别,主线程+TimeTick+superClient+基本4个线程池=7个线程
3. Session, 同Record, 多了一个 recordClient 线程
4. Scene, 同Session, 多了:sessionClient, miniClient, TCP客户端线程池去连接Function, TCP客户端连接池去连接 Blog. 注意客户端连接池,也是可扩张的,最基本是 1个连接处理线程+1个验证线程+1个OK处理线程,OK处理线程是可以扩张的. 没有recycle的原因是,即使丢失连接了,也不回收,只是不停地去连接,连接成功放入连接处理线程开始处理.
5. Gateway, 同Session, 多了: sessionClient, billClient, scenes对应的 sceneClients, minClient,TCP客户端线程池去连接Function, TCP客户端连接池去连接 Blog, TCP客户端连接池去连接Npc. 


### to be continued

1. 登陆流程
2. 进副本流程（创建副本）
3. 跨区流程
4. 玩家移动






















