添加拨号文件：
复制init.gprs-pppd和3gdata_call.conf脚本到andorid系统的/system/etc/目录。
adb push init.gprs-pppd /system/etc/
adb push 3gdata_call.conf /system/etc/
8200模块：
adb push 8200option /etc/ppp/peers/；
adb push 8200-chat /etc/ppp/chat/；

如果路径不存在，则需要自己创建,(拨号命令sudo pppd call 8200option)。
把对应文件的权限修改为777：
Adb shell chmod 777 /system/etc/init.gprs-pppd
Adb shell chmod 777 /system/etc/3gdata_call.conf 
8200模块：
adb shell chmod -R 777 /system/etc/peers
adb shell chmod -R 777 /system/etc/chat

