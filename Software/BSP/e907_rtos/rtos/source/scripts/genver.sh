DATETIME=`date -R`
commitid=`git rev-parse HEAD`
short_commitid=`git log -1 --pretty=format:'%h'`

if [ -d "${MELIS_BASE}/../../rtos-hal/" ] ; then
    cd ${MELIS_BASE}/../../rtos-hal/
    halcommitid=`git rev-parse HEAD`
    short_halcommitid=`git log -1 --pretty=format:'%h'`
    cd ${MELIS_BASE}
fi

echo $DATETIME > .ver1
echo \#define SDK_UTS_VERSION ' '\"`cat .ver1 `\" > .ver
echo \#define SDK_COMPILE_TIME   \"`date +%T`\" >> .ver
echo \#define SDK_COMPILE_BY '  '\"`whoami`\" >> .ver
echo \#define SDK_GIT_VERSION ' '\"$commitid\" >> .ver
echo \#define SDK_GIT_SHORT_VERSION ' '\"$short_commitid\" >> .ver
echo \#define TARGET_BOARD_TYPE ' '\"${TARGET_BOARD}\" >> .ver
echo \#define CPU_TYPE ' '\"$TARGET_PLATFORM\" >> .ver
if [ -d "${MELIS_BASE}/../../rtos-hal/" ] ; then
    echo \#define HAL_GIT_VERSION ' '\"${halcommitid}\" >> .ver
    echo \#define HAL_GIT_SHORT_VERSION ' '\"${short_halcommitid}\" >> .ver
fi

cat   .ver
rm -f .ver
