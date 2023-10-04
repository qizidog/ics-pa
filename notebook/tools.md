## vim-markdown

### install

尝试使用vim的markdown插件，坑还是有点多。

首先是在spacevim中启用 [`lang#markdown`](https://spacevim.org/cn/layers/lang/markdown/)，
[spacevim](https://spacevim.org/cn/documentation/)使用的插件管理工具是dein。目前对于vim的配置、插件管理都不熟悉，后面得补，现在先用起来再说。

```vim
[[layers]]
name = 'lang#markdown'
merge = false
```

该layer集成了 [iamcco/markdown-preview.nvim](https://github.com/iamcco/markdown-preview.nvim) 插件，需要 nodejs 和 yarn 支持，所以还要安装nodejs和yarn。

中途遇到报错

> opensslErrorStack: [ 'error:03000086:digital envelope routines::initializati......

原因是nodejs的版本太高了，重新装14版本的nodejs

```bash
# 添加nodejs的apt源（注意用的是14版本，非18最新版本）
ics2021 git:(pa1) curl -sL https://deb.nodesource.com/setup_14.x | sudo -E bash -

# 安装nodejs
sudo apt install nodejs

# 安装yarn
sudo npm install -g yarn

# 进入markdown-preview插件目录
cd ~/.cache/vimfiles/repos/github.com/iamcco/markdown-preview.nvim

# 安装插件
yarn install
yarn build

# 此后，在nvim中可以通过 `SPC l p` 快捷键预览markdown内容
```

### config

需求：希望能够ssh远程访问md的时候能通过本地浏览器来预览

由于用了spacevim，配置过程持续踩坑，后续需要系统学习下vim的配置和插件管理

目前的[解决方法](https://wsdjeg.spacevim.org/how-to-config-spacevim/)：

在~/.SpaceVim.d/init.toml的option下增加

```toml
# markdown-prewview.vim config
bootstrap_before = "myspacevim#before"
bootstrap_after = "myspacevim#after"
```

然后在 ~/.SpaceVim.d/下创建一个 `autoload` 目录，目录中增加文件 `myspacevim.vim`

```vim
" function! g:Open_browser(url)
    " silent exe '!lemonade open 'a:url
" endfunction

function! myspacevim#before() abort
    let g:mkdp_open_to_the_world = 1
    let g:mkdp_open_ip = '192.168.1.231'
    let g:mkdp_port = 8080
    let g:mkdp_echo_preview_url = 1
    " let g:mkdp_browserfunc = 'g:Open_browser'
endf

function! myspacevim#after() abort
endf
```

注意打开防火墙端口，虚拟机使用静态ip地址

可能不是最规范的配置方式，不过先用着。这里涉及到
[lemonade](https://github.com/lemonade-command/lemonade/releases/tag/v1.1.1)，
是一个实现虚拟机和宿主机复制粘贴的工具，需要[手动安装](https://github.com/iamcco/markdown-preview.nvim/pull/9)

[再次推荐共享粘贴板 Lemonade](https://zhuanlan.zhihu.com/p/65971135)

想要用好lemonade需要借助ssh端口转发（实现宿主机自动打开浏览器），我这里就懒得用了，选择通过点击echo_preview_url来打开链接（单个双引号是.vim类型文件的注释符号）

### md标签树

一般vim中的标签树都是用tagbar标签栏插件加上ctags标签插件来实现标。

总之先把tagbar功能打开（[文档](https://spacevim.org/cn/layers/gtags/)）。

```bash
# 查看ctags支持的标签类型
ctags --list-kinds | less
```

很不幸，在ctags支持的无数插件中并不包含markdown格式，所以又开始折腾之路。

在网上看到大家都推荐 [markdown2ctags](https://github.com/jszakmeister/markdown2ctags) 插件，
然而我尝试安装插件并完成必要配置之后标签树功能并没有生效，于是转而向spacevim文档寻求解决方案。

[文档](https://spacevim.org/cn/layers/lang/markdown/)要求安装php环境，或者自己手动安装mdctags（我选后者）。
[mdctags](https://github.com/wsdjeg/mdctags.rs)是rust编写的，没有提供现成的二进制程序，于是又安装rust环境手动编译。

```bash
# 安装rust环境
# 看到命令提示符后选择 1 default
curl https://sh.rustup.rs -sSf | sh
# 加载环境变量
source $HOME/.cargo/env
# 查看版本号
rustc -V
```

按照mdctags文档操作后，终于可以通过 `<F2>` 使用标签树了。

## linux使用webdav

连接webdav服务器的工具一般使用 `davfs2` 或者 `cadaver`

- [自动备份Linux上的博客数据到坚果云](https://chenyongjun.vip/articles/100)
- [linux同步webdav,Linux系统使用WebDAV自动挂载私有云盘](https://blog.csdn.net/weixin_30394975/article/details/116890505)
- [Linux下，挂载支持WebDav的网盘，扩展VPS空间](https://vps.yangmao.info/93753.html?btwaf=92820656)

将 `/etc/davfs2/davfs2.conf` 中的 **ignore_dav_header 0** 改为 **ignore_dav_header 1**，
否则下面的挂载操作会报：*mount.davfs: mounting failed; the server does not support WebDAV.*

创建目录 /mnt/dav，然后将坚果云的 /backup 目录挂载到 /mnt/dav。

```bash
# 方式一
mount.davfs https://dav.jianguoyun.com/dav/backup  /mnt/dav
# 方式二
mount -t davfs https://dav.jianguoyun.com/dav/backup  /mnt/dav
```

输入坚果云账户和应用密码即可，如果不想输入账户密码，
可在 /etc/davfs2/secrets 的最后面加一行：

```bash
https://dav.jianguoyun.com/dav/backup 坚果云账户 应用密码
```

此时，服务器、坚果云、本机三者的数据就都同步了。

如果遇到文件夹全是问号的情况，说明你之前挂载的目录非正常断开，
执行 `sudo umount -l /mnt/dav` 命令，即可恢复。

阅读 `davfs.conf` 可以发现，davfs默认使用50MB的缓存，缓存包括全局缓存和用户缓存，
用户缓存好像没有启用，只有实际用到的文件才会被缓存（连markdown中的图片都会选择性缓存，不知道怎么实现的），
所以不用担心每次挂载都需要消耗流量下载文件。

为了方便使用，可以准备一个shell脚本用于挂载dav，再在 `~` 目录下准备一个软链接关联到 `/mnt/dav`。

挂载dav后如果没有umount，关机的时候会卡死，所以需要写一个系统服务，确保关机时自动取消dav的挂载。

[如何让ubuntu在关机或重启时执行脚本](https://blog.csdn.net/weixin_43230275/article/details/118977052)

```bash
# 创建一个系统服务
root@root:~# vim /etc/systemd/system/dav_umount.service

[Unit]
Description=Umount /mnt/dav at shutdown
Requires=network.target
DefaultDependencies=no
Conflicts=reboot.target
Before=shutdown.target
[Service]
Type=oneshot
RemainAfterExit=true
ExecStart=/bin/true
ExecStop=/bin/bash /home/vbox/.dav_umount.sh
##上面写上你的脚本绝对路径
[Install]
WantedBy=multi-user.target

# 启用该系统服务
root@root:~# chmod 755 /etc/systemd/system/dav_umount.service
root@root:~# systemctl daemon-reload
root@root:~# systemctl enable /etc/systemd/system/dav_umount.service
root@root:~# systemctl status dav_umount.service  # 应该会看到active，如果没有的话就手动start一下
```

## vnc+xfce4连接远程服务器

> vnc+xfce4连接服务器成功后执行nume发生警告提示"Xlib:  extension "XInputExtension" missing on display ":1.0"."

警告产生原因是使用了tightvncserver。tightvncserver最后的版本维持在1.3.10，是2009年的版本，推荐使用[TigerVNC](https://github.com/TigerVNC/tigervnc/releases)，基于RealVNC 4和X.org，并在2009年脱离父项目TightVNC。

> 改用TigerVNC后执行`vncserver`命令报错

是由 `~/.vnc/xstartup` 导致的，xstartup中可以直接定义需要启用的桌面环境，之前使用xtartup的是：

```bash
#!/bin/bash
startxfce4 &
```
现在需要改成：
```bash
#!/bin/bash
xfce4-session
```
最后准备了两个快捷命令：
```bash
alias vncstart="vncserver -localhost -geometry 1080x640 :1"
alias vnckill="vncserver -kill :1"
```
`-localhost` 参数要借助 `alias svnc="ssh -L 5901:localhost:5901 -C -N alicloud"` 来打通远程服务器之间的端口（可以避免开放防火墙）。
