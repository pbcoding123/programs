## Lock Screen

在机房被 JC？赶紧用这款开源简洁的锁屏吧

每次启动时会自动 Hook 键盘，并将 Ctrl+Alt+Delete 中的所有选项删掉，没有后门，关闭时也会自动还原。

## 配置

在目录下的 lock.cfg 共有 $4$ 行，分别表示：

* 主标题
* 解锁按钮文字
* 密码错误提示
* 密码

以 base64 存储。

若没有 lock.cfg 或没有第四行，密码为默认的 `secret123`。

## 编译命令

```batch
g++ -o lock.exe lock.cpp -O2 -std=c++14 -lgdi32 -mwindows
```
