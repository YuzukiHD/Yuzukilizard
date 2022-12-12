# Tutorial on Using BSP Development Kit

Before using, please refer to this article to download the official Tina Linux image provided by Allwinner: 

- https://v853.docs.aw-ol.com/en/study/study_3getsdk/

![image](https://user-images.githubusercontent.com/12003087/204969926-d6a11bcb-a8ac-40fd-add8-8049d0ae8046.png)

After downloading the official Tina Linux, overwrite the files in this folder to the root directory of the official Tina Linux development package

![image](https://user-images.githubusercontent.com/12003087/204970083-ab2d7883-a8c0-439f-9996-c75c85223d02.png)

Finally, go to the patch folder and apply the following patch

## WSL 2 

If the compilation host uses WSL2, please clean up the environment variables and reserve the minimum environment variables, otherwise the following error will occur

```
./etc/init.d/rcS: line 37: /proc/sys/kernel/printk: Permission denied
```

You can use this command to clean up

```
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/usr/lib/wsl/lib"
```