./testOnvif $(ifconfig | grep 192 | awk '{print $2}' | awk -F ':' '{print $2}')
