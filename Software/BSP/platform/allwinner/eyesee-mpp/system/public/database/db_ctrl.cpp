#include <database/db_ctrl.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <string.h>
#include <errno.h>

#undef LOG_NDEBUG
#undef NDEBUG
#undef LOG_TAG
#define LOG_TAG "DBCtrl"
#include <log/log_wrapper.h>

using namespace std;
using namespace EyeseeLinux;

#define RETRY_CNT 3

#define ERR_TIMEOUT -2

#define BEGIN_TRANSACTION "begin transaction"
#define COMMIT_TRANSACTION "commit transaction"

DBCtrl::DBCtrl()
	:mName(""),
	mSQL(""),
	mColumnCnt(0),
	mTmpNum(0),
	mColumnType(NULL),
	mColumnTypeSize(NULL),
	mValue(NULL),
	mColumnName(NULL),
	mFieldKey(NULL),
	mDBCon(NULL),
	mSqlstmt(NULL),
	mFilter("")

{
}

int DBCtrl::init()
{
    if (access(DB_PATH, F_OK) < 0) {
        int ret = -1;
        if ( (ret = mkdir(DB_PATH, 0755)) < 0) {
            LOGE("mkdir %s failed, %s", DB_PATH, strerror(errno));
            return ret;
        }
    }
	mDBCon  = DBCon::getInstance(DB_FILE);
	mSQLCon = mDBCon->getConnect();
    return 0;
}

DBCtrl::~DBCtrl()
{
	if(mDBCon) {
		LOGD("freeInstance\n");
		mDBCon->freeInstance();
		mDBCon   = NULL;
	}

	mSqlstmt = NULL;
	mSQLCon  = NULL;
	if (mColumnType) {
		delete [] mColumnType;
		mColumnType = NULL;
	}
	if (mColumnName) {
		delete [] mColumnName;
		mColumnName = NULL;
	}
	if (mColumnTypeSize) {
		delete [] mColumnTypeSize;
		mColumnTypeSize = NULL;
	}
	if (mValue) {
		delete [] mValue;
		mValue = NULL;
	}
	if (mFieldKey) {
		delete [] mFieldKey;
		mFieldKey = NULL;
	}
}

void DBCtrl::setTableName(const string name)
{
	mName = name;
}

string DBCtrl::getTableName(void) const
{
	return mName;
}

void DBCtrl::setSQL(const string sql)
{
	mSQL = sql;
}

string DBCtrl::getSQL(void) const
{
	return mSQL;
}

void DBCtrl::setRecord(string columnName, void *pValue)
{
	if(mTmpNum >= mColumnCnt) {
		LOGE("%s():[%d] invalid mTmpNum: [%d]\n", __FUNCTION__, __LINE__, mTmpNum);
		return;
	}
	mColumnName[mTmpNum] = columnName;
	mValue[mTmpNum] = (unsigned int)pValue;		/* save the column adress */
	mTmpNum++;
}

void DBCtrl::setColumnType(DBC_TYPE *type, int *typeSize)
{
	int i=0;
	while (i < mColumnCnt) {
		mColumnType[i] = type[i];
		i++;
	}
	if (0 != typeSize) { /* just for DB_BLOB */
		i = 0;
		while (i < mColumnCnt) {
			mColumnTypeSize[mTmpNum] = typeSize[i];
			i++;
		}
	}
}

int DBCtrl::insertRecord(void)
{
	int cnt = 0;

	if (!mSQLCon) {
		LOGE("mSQLCon is null\n");
		return -1;
	}
	createAddSQL();
	if (mSQL.empty()) {
		LOGE("mSQL is empty\n");
		return -1;
	}
	LOGD("SQL is %s\n", mSQL.c_str());
	while(mDBCon->lock()) {
		if (cnt++ > RETRY_CNT) {
			return ERR_TIMEOUT;
		} else {
			usleep(200000);
		}
	}
	int errorCode;
	errorCode = sqlite3_prepare(mSQLCon, mSQL.c_str(), -1, &mSqlstmt, NULL);
	if (SQLITE_OK != errorCode) {
		ERROR(__FUNCTION__, __LINE__, mSQLCon);
		return errorCode;
	}
	int idx = 0;
	int i;
	sqlite3_exec(mSQLCon, BEGIN_TRANSACTION, NULL, NULL, NULL);

	for (i=1; idx<mColumnCnt; idx++,i++) {
		switch(mColumnType[idx]) {
		case DB_UINT32:
			sqlite3_bind_int(mSqlstmt, i, *(unsigned int*)mValue[idx]);
			break;
		case DB_UINT16:
			sqlite3_bind_int(mSqlstmt, i, *(unsigned short*)mValue[idx]);
			break;
		case DB_UINT8:
			LOGD("mValue[idx] is %d\n", *(unsigned char *)mValue[idx]);
			sqlite3_bind_int(mSqlstmt, i, *(unsigned char*)mValue[idx]);
			break;
		case DB_INT64:
			sqlite3_bind_int64(mSqlstmt, i, *(long long*)mValue[idx]);
			break;
		case DB_TEXT:
			sqlite3_bind_text(mSqlstmt, i, (char *)mValue[idx], -1, NULL);
			break;
		case DB_FLOAT:
			sqlite3_bind_double(mSqlstmt, i, *(float*)mValue[idx]);
			break;
		case DB_DOUBLE:
			sqlite3_bind_double(mSqlstmt, i, *(double*)mValue[idx]);
			break;
		case DB_BLOB:
			sqlite3_bind_blob(mSqlstmt, i, (void*)mValue[idx], mColumnTypeSize[idx], NULL);
			break;
		default:
			break;
		}
	}

	errorCode = sqlite3_step(mSqlstmt);
	if (SQLITE_DONE != errorCode) {
		ERROR(__FUNCTION__, __LINE__, mSQLCon);
		sqlite3_finalize(mSqlstmt);
		mDBCon->unlock();
		mTmpNum = 0;	/* set to default value */
		sqlite3_exec(mSQLCon, COMMIT_TRANSACTION, NULL, NULL, NULL);
		return errorCode;
	}

	sqlite3_finalize(mSqlstmt);
	mDBCon->unlock();
	mTmpNum = 0;	/* set to default value */
	sqlite3_exec(mSQLCon, COMMIT_TRANSACTION, NULL, NULL, NULL);
	return SQLITE_OK;
}

int DBCtrl::bufPrepare(void)
{
	if(mColumnCnt <= 0)
		return -1;
	queryBuf = new char[mColumnCnt][100];

	return 0;
}

void DBCtrl::bufRelease(void)
{
	if(queryBuf) {
		delete []queryBuf;
		queryBuf = NULL;
	}
}


int DBCtrl::prepare(void)
{
	int cnt = 0;

	if (!mSQLCon) {
		LOGE("mSQLCon is null\n");
		return -1;
	}
	createRequerySQL();
	if (mSQL.empty()) {
		LOGE("mSQL is empty\n");
		return -1;
	}
	while(mDBCon->lock()) {
		if (cnt++ > RETRY_CNT) {
			return ERR_TIMEOUT;
		} else {
			usleep(200000);
		}
	}
	const char *errorMsg = NULL;
	int errorCode;
	LOGD("SQL is %s\n", mSQL.c_str());
	errorCode = sqlite3_prepare(mSQLCon, mSQL.c_str(), -1, &mSqlstmt, &errorMsg);
	if (SQLITE_OK != errorCode) {
		ERROR(__FUNCTION__, __LINE__, mSQLCon);
		mDBCon->unlock();
		return -1;
	}

	if(bufPrepare() == -1) {
		mDBCon->unlock();
		return -1;
	}

	mDBCon->unlock();
	return 0;
}

int DBCtrl::finish(void)
{
	int cnt = 0;

	if (!mSQLCon) {
		LOGE("mSQLCon is null\n");
		return -1;
	}
	if (mSQL.empty()) {
		LOGE("mSQL is empty\n");
		return -1;
	}
	while(mDBCon->lock()) {
		if (cnt++ > RETRY_CNT) {
			return ERR_TIMEOUT;
		} else {
			usleep(200000);
		}
	}
	sqlite3_finalize(mSqlstmt);
	bufRelease();

	mDBCon->unlock();
	mFilter = "";
	return 0;
}



int DBCtrl::deleteRecord(void)
{
	mSQL = "DELETE FROM ";
	mSQL.append(mName);
	mSQL.append(" ");
	mSQL.append(mFilter);
	mFilter = "";
	return 	executeSQL();
}

int DBCtrl::executeSQL(void)
{
	int cnt = 0;
	while(mDBCon->lock()) {
		if (cnt++ > RETRY_CNT) {
			return ERR_TIMEOUT;
		} else {
			usleep(200000);
		}
	}
	char *errorMsg = NULL;
	int errorCode = sqlite3_exec(mSQLCon, BEGIN_TRANSACTION, NULL, NULL, &errorMsg);
	if (SQLITE_OK != errorCode) {
		ERROR(__FUNCTION__, __LINE__, mSQLCon);
		mDBCon->unlock();
		return errorCode;
	}
	errorCode = sqlite3_exec(mSQLCon, mSQL.c_str(), NULL, NULL, &errorMsg);
	if (SQLITE_OK != errorCode) {
		ERROR(__FUNCTION__, __LINE__, mSQLCon);
//		return errorCode;
	}
	errorCode = sqlite3_exec(mSQLCon, COMMIT_TRANSACTION, NULL, NULL, &errorMsg);
	if (SQLITE_OK != errorCode) {
		mDBCon->unlock();
		ERROR(__FUNCTION__, __LINE__, mSQLCon);
	}
	mDBCon->unlock();
	return errorCode;
}

int DBCtrl::sqlite3Exec(void)
{
	int cnt = 0;
	while (mDBCon->lock()) {
		if (cnt++ > RETRY_CNT) {
			return ERR_TIMEOUT;
		} else {
			usleep(200000);
		}
	}
	char *errorMsg = NULL;
	int errorCode = sqlite3_exec(mSQLCon, mSQL.c_str(), NULL, NULL, &errorMsg);
	if (SQLITE_OK != errorCode) {
		ERROR(__FUNCTION__, __LINE__, mSQLCon);
		return errorCode;
	}
	mDBCon->unlock();
	return errorCode;
}

unsigned int DBCtrl::getRecordCnt(void)
{
	if(!mSQLCon) {
		LOGE("mSQLCon is null\n");
		return -1;
	}
	string str("SELECT COUNT(*) FROM ");
	str.append(mName);
	str.append(" ");
	str.append(mFilter);
	setSQL(str);
	if (mSQL.empty()) {
		LOGE("mSQL is empty\n");
		return -1;
	}
	int cnt = 0;
	while (mDBCon->lock()) {
		if (cnt++ > RETRY_CNT) {
			mFilter = "";
			return ERR_TIMEOUT;
		} else {
			usleep(200000);
		}
	}
	const char *errorMsg = NULL;
	int errorCode = sqlite3_prepare(mSQLCon, mSQL.c_str(), -1, &mSqlstmt, &errorMsg);
	if (SQLITE_OK != errorCode) {
		mDBCon->unlock();
	}
	int recordNum;
	errorCode = sqlite3_step(mSqlstmt);
	if (SQLITE_ROW == errorCode) {
		recordNum = (unsigned int)sqlite3_column_int(mSqlstmt, 0);
	} else {
		recordNum = -1;
		if (SQLITE_DONE != errorCode) {
			mFilter = "";
			ERROR(__FUNCTION__, __LINE__, mSQLCon);
		}
	}
	sqlite3_finalize(mSqlstmt);
	mDBCon->unlock();
	mFilter = "";
	return recordNum;

}

int DBCtrl::getOneLine(void *pValue[])
{
	const char *errorMsg = NULL;
	int errorCode;
	int cnt = 0;
	while (mDBCon->lock()) {
		if (cnt++ > RETRY_CNT) {
			mFilter = "";
			return ERR_TIMEOUT;
		} else {
			usleep(200000);
		}
	}
	const void *tmpBlob = NULL;
	int len, i;
	int idx;
	const unsigned char *tmpStr;
	errorCode = sqlite3_step(mSqlstmt);
	if (SQLITE_ROW == errorCode) {
		for (idx=0; idx < mColumnCnt; idx++) {
			switch(mColumnType[idx]) {
			case DB_UINT32:
				*(unsigned int *)&pValue[idx] = (unsigned int)sqlite3_column_int(mSqlstmt, idx);
				break;
			case DB_INT64:
				*(long long *)&pValue[idx] = (long long)sqlite3_column_int64(mSqlstmt, idx);
				break;
			case DB_UINT16:
				*(unsigned short *)&pValue[idx] = (unsigned short)sqlite3_column_int(mSqlstmt, idx);
				pValue[idx] = (void *)&result_uint16;
				break;
			case DB_UINT8:
				*(unsigned char *)&pValue[idx] = (unsigned char)sqlite3_column_int(mSqlstmt, idx);
				break;
			case DB_TEXT:
				tmpStr = sqlite3_column_text(mSqlstmt, idx);
				if (tmpStr) {
					memcpy(queryBuf[idx], tmpStr, strlen((const char *)tmpStr) + 1);
					pValue[idx] = queryBuf[idx];
				} else {
					errorCode = -1;
				}
				break;
			case DB_FLOAT:
				*(float *)&pValue[idx] = (float)sqlite3_column_double(mSqlstmt, idx);
				break;
			case DB_DOUBLE:
				*(double *)&pValue[idx] = sqlite3_column_double(mSqlstmt, idx);
				break;
#if 0
			case DB_BLOB:
				tmpBlob = sqlite3_column_blob(mSqlstmt, idx);
				len = sqlite3_column_bytes(mSqlstmt, idx);
				memcpy((void *)pValue, tmpBlob, len);
				break;
#endif
			default:
				break;
			}
		}
	}
	mDBCon->unlock();
	return errorCode;
}

/* 
 * 查询满足Filter条件的第一条记录
 * 返回值： 返回满足Filter条件的记录的数量，返回-1表示查询失败！
 * */
int DBCtrl::getFilterFirstLine(void *pValue[])
{
	int errorCode;

	string str("SELECT * FROM ");
	str.append(mName);
	str.append(" ");
	str.append(mFilter);
	str.append(" ");
	str.append("LIMIT 0,1");
	setSQL(str);

	if (mSQL.empty()) {
		LOGE("mSQL is empty\n");
		return -1;
	}
	int cnt = 0;
	while (mDBCon->lock()) {
		if (cnt++ > RETRY_CNT) {
			mFilter = "";
			return ERR_TIMEOUT;
		} else {
			usleep(200000);
		}
	}

	const char *errorMsg = NULL;
	errorCode = sqlite3_prepare(mSQLCon, mSQL.c_str(), -1, &mSqlstmt, &errorMsg);
	if (SQLITE_OK != errorCode) {
		mDBCon->unlock();
	}

	int recordNum;
	int idx;
	const void *tmpBlob;
	char *tmpStr;
	int len;
	errorCode = sqlite3_step(mSqlstmt);
	if (SQLITE_ROW == errorCode) {
		for (idx=0; idx < mColumnCnt; idx++) {
			switch(mColumnType[idx]) {
			case DB_UINT32:
				*(unsigned int *)&pValue[idx] = (unsigned int)sqlite3_column_int(mSqlstmt, idx);
				break;
			case DB_INT64:
				*(long long *)&pValue[idx] = (long long)sqlite3_column_int64(mSqlstmt, idx);
				break;
			case DB_UINT16:
				*(unsigned short *)&pValue[idx] = (unsigned short)sqlite3_column_int(mSqlstmt, idx);
				break;
			case DB_UINT8:
				*(unsigned char *)&pValue[idx] = (unsigned char)sqlite3_column_int(mSqlstmt, idx);
				break;
			case DB_TEXT:
				tmpStr = (char *)sqlite3_column_text(mSqlstmt, idx);
				if (tmpStr) {
					memcpy(queryBuf[idx], tmpStr, strlen(tmpStr) + 1);
					pValue[idx] = &queryBuf[idx];
				} else {
					errorCode = -1;
				}
				break;
			case DB_FLOAT:
				*(float *)&pValue[idx] = (float)sqlite3_column_double(mSqlstmt, idx);
				break;
			case DB_DOUBLE:
				*(double *)&pValue[idx] = sqlite3_column_double(mSqlstmt, idx);
				break;
#if 0
			case DB_BLOB:
				tmpBlob = sqlite3_column_blob(mSqlstmt, idx);
				len = sqlite3_column_bytes(mSqlstmt, idx);
				memcpy((void *)pValue, tmpBlob, len);
				break;
#endif
			default:
				LOGD("not matched type\n");
				sqlite3_finalize(mSqlstmt);
				mDBCon->unlock();
				mFilter = "";
				return -1;
			}
		}
	} else {
		ERROR(__FUNCTION__, __LINE__, mSQLCon);
		sqlite3_finalize(mSqlstmt);
		mDBCon->unlock();
		mFilter = "";
		return -1;
	}

	sqlite3_finalize(mSqlstmt);
	mDBCon->unlock();
//	mFilter = "";	/* filter will be still used in getRecordCnt */

	return getRecordCnt();
}

/* 
 * idx: 列在mColumnType中的索引
 * 查询满足Filter条件的第一个元素的地址
 * 返回值： 返回NULL表示查询失败！
 * */
void * DBCtrl::getFilterFirstElement(string column, int idx)
{
	int errorCode;
	void *retAddr = NULL;

	string str("SELECT ");
	str.append(column);
	str.append(" ");
	str.append("FROM ");
	str.append(mName);
	str.append(" ");

	str.append(mFilter);
	str.append(" ");
	str.append("LIMIT 0,1");
	setSQL(str);

	if (mSQL.empty()) {
		LOGE("mSQL is empty\n");
		return NULL;
	}
	int cnt = 0;
	while (mDBCon->lock()) {
		if (cnt++ > RETRY_CNT) {
			mFilter = "";
			return NULL;
		} else {
			usleep(200000);
		}
	}

	const char *errorMsg = NULL;
	errorCode = sqlite3_prepare(mSQLCon, mSQL.c_str(), -1, &mSqlstmt, &errorMsg);
	if (SQLITE_OK != errorCode) {
		mDBCon->unlock();
	}

	int recordNum;
	const void *tmpBlob;
	char *tmpStr;
	int len;
	errorCode = sqlite3_step(mSqlstmt);
	if (SQLITE_ROW == errorCode) {
			switch(mColumnType[idx]) {
			case DB_UINT32:
				result_uint32 = (unsigned int)sqlite3_column_int(mSqlstmt, 0);
				retAddr =  (void *)&result_uint32;
				break;
			case DB_INT64:
				result_int64 = (long long)sqlite3_column_int64(mSqlstmt, 0);
				retAddr =  (void *)&result_int64;
				break;
			case DB_UINT16:
				result_uint16 = (unsigned short)sqlite3_column_int(mSqlstmt, 0);
				retAddr =  (void *)&result_uint16;
				break;
			case DB_UINT8:
				result_uint8 = (unsigned char)sqlite3_column_int(mSqlstmt, 0);
				retAddr =  (void *)&result_uint8;
				break;
			case DB_TEXT:
				tmpStr = (char *)sqlite3_column_text(mSqlstmt, 0);
				if (tmpStr) {
					memcpy(queryBuf[idx], tmpStr, strlen(tmpStr) + 1);
					retAddr =  &queryBuf[idx];
				} else {
					retAddr = NULL;
				}
				break;
			case DB_FLOAT:
				result_float = (float)sqlite3_column_double(mSqlstmt, 0);
				retAddr =  (void *)&result_float;
				break;
			case DB_DOUBLE:
				result_double = sqlite3_column_double(mSqlstmt, 0);
				retAddr =  (void *)&result_double;
				break;
#if 0
			case DB_BLOB:
				tmpBlob = sqlite3_column_blob(mSqlstmt, 0);
				len = sqlite3_column_bytes(mSqlstmt, 0);
				memcpy((void *)pValue, tmpBlob, len);
				break;
#endif
			default:
				retAddr =  NULL;
				break;
			}
	} else {
		ERROR(__FUNCTION__, __LINE__, mSQLCon);
	}

	sqlite3_finalize(mSqlstmt);
	mDBCon->unlock();
	mFilter = "";
	return retAddr;
}

/*
 * Get the specified element, not use filter
 * Parameter: columnName:   the column name int the schema table
 *			  columnIdx:	the column idx in the sql_tb
 *			  rowNumber:	row index, start from 0
 * */
void * DBCtrl::getElement(string columnName, int columnIdx, unsigned int rowNumber)
{
	int errorCode;
	void *retAddr = NULL;
	char buf[20];

	bzero(buf, sizeof(buf));
	sprintf(buf, "LIMIT %d,1", rowNumber);
	string str("SELECT ");
	str.append(columnName);
	str.append(" ");
	str.append("FROM ");
	str.append(mName);
	str.append(" ");
	str.append(mFilter);
	str.append(" ");
	str.append(buf);
	setSQL(str);

	if (mSQL.empty()) {
		LOGE("mSQL is empty\n");
		return NULL;
	}
	int cnt = 0;
	while (mDBCon->lock()) {
		if (cnt++ > RETRY_CNT) {
			mFilter = "";
			return NULL;
		} else {
			usleep(200000);
		}
	}

	const char *errorMsg = NULL;
	errorCode = sqlite3_prepare(mSQLCon, mSQL.c_str(), -1, &mSqlstmt, &errorMsg);
	if (SQLITE_OK != errorCode) {
		mDBCon->unlock();
	}

	int recordNum;
	const void *tmpBlob;
	char *tmpStr;
	int len;
	errorCode = sqlite3_step(mSqlstmt);
	if (SQLITE_ROW == errorCode) {
			switch(mColumnType[columnIdx]) {
			case DB_UINT32:
				result_uint32 = (unsigned int)sqlite3_column_int(mSqlstmt, 0);
				retAddr =  (void *)&result_uint32;
				break;
			case DB_INT64:
				result_int64 = (long long)sqlite3_column_int64(mSqlstmt, 0);
				retAddr =  (void *)&result_int64;
				break;
			case DB_UINT16:
				result_uint16 = (unsigned short)sqlite3_column_int(mSqlstmt, 0);
				retAddr =  (void *)&result_uint16;
				break;
			case DB_UINT8:
				result_uint8 = (unsigned char)sqlite3_column_int(mSqlstmt, 0);
				retAddr =  (void *)&result_uint8;
				break;
			case DB_TEXT:
				tmpStr = (char *)sqlite3_column_text(mSqlstmt, 0);
				if (tmpStr) {
					memcpy(queryBuf[columnIdx], tmpStr, strlen(tmpStr));
					queryBuf[columnIdx][strlen(tmpStr)] = '\0';
					retAddr =  &queryBuf[columnIdx];
				} else {
					retAddr = NULL;
				}
				break;
			case DB_FLOAT:
				result_float = (float)sqlite3_column_double(mSqlstmt, 0);
				retAddr =  (void *)&result_float;
				break;
			case DB_DOUBLE:
				result_double = sqlite3_column_double(mSqlstmt, 0);
				retAddr =  (void *)&result_double;
				break;
#if 0
			case DB_BLOB:
				tmpBlob = sqlite3_column_blob(mSqlstmt, 0);
				len = sqlite3_column_bytes(mSqlstmt, 0);
				memcpy((void *)pValue, tmpBlob, len);
				break;
#endif
			default:
				retAddr =  NULL;
				break;
			}
	} else {
		retAddr = NULL;
		ERROR(__FUNCTION__, __LINE__, mSQLCon);
	}

	sqlite3_finalize(mSqlstmt);
	mDBCon->unlock();
	return retAddr;
}


void DBCtrl::setColumnCnt(const int num)
{
	mColumnCnt = num;
	mColumnName = new string[num];
	mColumnType = new DBC_TYPE[num];
	mColumnTypeSize = new int[num];
	mValue = new unsigned int[num];
	mFieldKey = new unsigned char[num];
}

void DBCtrl::createRequerySQL(void)
{
	int i = 0;
	string str("SELECT ");
	mSQL = str;
	for (i=0; i<mColumnCnt-1; i++) {
		mSQL.append(mColumnName[i]);
		mSQL.append(", ");
	}
	mSQL.append(mColumnName[mColumnCnt-1]);
	mSQL.append(" FROM ");
	mSQL.append(mName);
	mSQL.append(" ");
	mSQL.append(mFilter);
	mFilter = "";
}

void DBCtrl::createAddSQL(void)
{
	int i = 0;
	string str1("INSERT INTO ");
	string str2(" VALUES(");
	str1.append(mName);
	str1.append("(");

	for (; i<mColumnCnt-1; i++) {
		str1.append(mColumnName[i]);
		str1.append(", ");
		str2.append("?, ");
	}
	str1.append(mColumnName[mColumnCnt-1]);
	str1.append(") ");
	str2.append("?)");
	mSQL = str1;
	mSQL.append(str2);
	mFilter = "";
}

int DBCtrl::createTable(string tbName, struct sql_tb *tb, int cnt)
{
	char *errorMsg = NULL;
	string str("CREATE TABLE IF NOT EXISTS ");
	setTableName(tbName);
	str.append(mName);
	str.append("(");
	int i = 0;
	int errorCode;

	while (mDBCon->lock()) {
		if (cnt++ > RETRY_CNT) {
			mDBCon->unlock();
			return ERR_TIMEOUT;
		} else {
			usleep(200000);
		}
	}
	for (i=0; i<cnt-1; i++) {
		str.append(tb[i].item);
		str.append(" ");
		str.append(tb[i].type);
		if( i == 0) {
			str.append(" PRIMARY KEY");
		}
		str.append(", ");
	}
	str.append(tb[i].item);
	str.append(" ");
	str.append(tb[i].type);
	str.append(");");
	errorCode = sqlite3_exec(mSQLCon, str.c_str(), NULL, NULL, &errorMsg);
    if(SQLITE_OK != errorCode) {
		ERROR(__FUNCTION__, __LINE__, mSQLCon);
        goto out;
	}
	setColumnCnt(cnt);
out:
	mDBCon->unlock();
    return errorCode;
}

int DBCtrl::deleteTable(string tbName)
{
	char *errorMsg = NULL;
	int errorCode;
	string str("DROP TABLE IF EXISTS ");
	str += mName;
	errorCode = sqlite3_exec(mSQLCon, str.c_str(), NULL, NULL, &errorMsg);
	if(SQLITE_OK != errorCode) {
		ERROR(__FUNCTION__, __LINE__, mSQLCon);
	}

	if (mColumnType) {
		delete [] mColumnType;
		mColumnType = NULL;
	}
	if (mColumnName) {
		delete [] mColumnName;
		mColumnName = NULL;
	}
	if (mColumnTypeSize) {
		delete [] mColumnTypeSize;
		mColumnTypeSize = NULL;
	}
	if (mValue) {
		delete [] mValue;
		mValue = NULL;
	}
	if (mFieldKey) {
		delete [] mFieldKey;
		mFieldKey = NULL;
	}

	return errorCode;
}

void DBCtrl::setFilter(const string filter)
{
	mFilter = filter;
}
