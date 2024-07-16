/*
WXBase 基础库，基于C++11
*/
#ifndef _WX_BASE_H_
#define _WX_BASE_H_

#include  <stdint.h>
#include  <stdarg.h>
#include  <thread>
#include  <mutex>
#include  <queue>
#include  <fstream>
#include  <codecvt>
#include <future>

#include <functional>

#ifdef _WIN32
#include <Windows.h>
#define  RENAME MoveFileA
#include <tchar.h>
#else
#define  RENAME rename
#define _T(x)       x
#define _TEXT(x)    x
using namespace std;
#endif

//递归锁
#define WXLocker    std::recursive_mutex

//自动锁
#define WXAutoLock  std::lock_guard<WXLocker>

//内存数据管理
class WXDataBuffer {
public:
	uint8_t *m_pBuf = nullptr;
	int     m_iBufSize = 0;
	int     m_iPos = 0;
	int64_t extra1 = 0;
	int64_t extra2 = 0;
public:
	void Init(uint8_t *buf, int size) {
		if (m_pBuf)delete[]m_pBuf;
		m_pBuf = new uint8_t[size];
		if (buf != nullptr) {
			memcpy(m_pBuf, buf, size);
		}else {
			memset(m_pBuf, 0, size);
		}
		m_iBufSize = size;
	}

	WXDataBuffer() {}

	WXDataBuffer(uint8_t *buf, int size) {
		Init(buf, size);
	}

	virtual ~WXDataBuffer() {
		if (m_pBuf) {
			delete[]m_pBuf;
			m_pBuf = nullptr;
			m_iBufSize = 0;
		}
	}
};

class WXString {
	std::string  m_strUTF8 = "";//ffmpeg 需要
	std::wstring m_strUnicode = L"";

	std::wstring ANSIToUnicode(const std::string & str) {
		std::wstring ret;
		std::mbstate_t state = {};
		const char *src = str.data();
		size_t len = std::mbsrtowcs(nullptr, &src, 0, &state);
		if (static_cast<size_t>(-1) != len) {
			std::unique_ptr< wchar_t[] > buff(new wchar_t[len + 1]);
			len = std::mbsrtowcs(buff.get(), &src, len, &state);
			if (static_cast<size_t>(-1) != len) {
				ret.assign(buff.get(), len);
			}
		}
		return ret;
	}

	std::string  UnicodeToUTF8(const std::wstring & wstr){
		std::wstring_convert< std::codecvt_utf8<wchar_t> > wcv;
		std::string	ret = wcv.to_bytes(wstr);
		return ret;
	}

	//Acsi
	void InitA(const char *sz) {
#ifdef _WIN32
		std::string strAsci = sz;
		m_strUnicode = ANSIToUnicode(strAsci);
		m_strUTF8 = UnicodeToUTF8(m_strUnicode);
#else
        m_strUTF8 = sz;
        std::wstring_convert< std::codecvt_utf8<wchar_t> > wcv;
        m_strUnicode = wcv.from_bytes(sz);
#endif
	}

	//Unicode
	void InitW(const wchar_t* wsz) {
		m_strUnicode = wsz;
		m_strUTF8 = UnicodeToUTF8(m_strUnicode);
	}

public:
	void Format(const char * format, ...) {
		char    szMsg[4096];
		memset(szMsg, 0, 4096);
		va_list marker
#ifdef  _WIN32
        = nullptr
#endif
        ;
		va_start(marker, format);
		vsprintf(szMsg, format, marker);
		va_end(marker);
		InitA(szMsg);
	}
	void Format(const wchar_t * format, ...) {
		wchar_t wszMsg[4096];
		memset(wszMsg, 0, 4096 * 2);
        va_list marker
#ifdef  _WIN32
        = nullptr
#endif
        ;
		va_start(marker, format);
		vswprintf(wszMsg,
#ifndef _WIN32
              4096,
#endif
         format, marker);
		va_end(marker);
		InitW(wszMsg);
	}
public:
	WXString() {}

	WXString(const WXString& wxstr) {
		InitW(wxstr.w_str());
	}

	WXString(const wchar_t* wsz) {
		InitW(wsz);
	}

	const int  length() const {
		return (int)m_strUnicode.length();
	}
    
#ifdef _WIN32
	const wchar_t* str()const  {
		return w_str();
	}
    const wchar_t* Left(int n) {
        return m_strUnicode.c_str() + (length() - n);
    }
#else
    const char* str()const  {
        return c_str();
    }
    const char* Left(int n) {
        return m_strUTF8.c_str() + (m_strUTF8.length() - n);
    }
#endif
    
    const wchar_t* w_str()const  {
        return m_strUnicode.c_str();
    }
    
	const char*  c_str() const {
		return m_strUTF8.c_str();
	}


public://WXString

	WXString& operator+=(const WXString& wxstr) {
		std::wstring wstr = m_strUnicode;
		wstr += wxstr.w_str();
		InitW(wstr.c_str());
		return *this;
	}
	bool operator==(const WXString& wxstr) const {
		const wchar_t* wsz1 = this->w_str();
		const wchar_t* wsz2 = wxstr.w_str();
		return (wcscmp(wsz1, wsz2) == 0);
	}
	bool operator!=(const WXString& wxstr) const {
		const wchar_t* wsz1 = this->w_str();
		const wchar_t* wsz2 = wxstr.w_str();
		return (wcscmp(wsz1, wsz2) != 0);
	}

public://const wchar_t*
	WXString& operator+=(const wchar_t*  wsz) {
		std::wstring wstr = m_strUnicode;
		wstr += wsz;
		InitW(wstr.c_str());
		return *this;
	}
	bool operator==(const wchar_t* wsz) const {
		const wchar_t* wsz1 = this->w_str();
		return (wcscmp(wsz1, wsz) == 0);
	}
	bool operator!=(const wchar_t* wsz) const {
		const wchar_t* wsz1 = this->w_str();
		return (wcscmp(wsz1, wsz) != 0);
	}
	void Cat(WXString wxstr, const wchar_t* wszSlipt) { 
		//添加一个间隔符来拼接字符串
		if (wxstr.length() != 0) {
			std::wstring wstr = m_strUnicode;
			if (length() != 0)
				wstr += wszSlipt;
			wstr += wxstr.w_str();
			InitW(wstr.c_str());
		}
	}
};

//成员应该为指针类型！
template <class PtrT>
class ThreadSafeQueue {
protected:
	WXLocker m_lock;
	std::queue<PtrT> m_queue;
public:
	bool Empty() {
		WXAutoLock al(m_lock);
		return m_queue.empty();
	}
	PtrT Pop() {
		PtrT obj = nullptr;
		{
			WXAutoLock al(m_lock);
			if (!m_queue.empty()) {
				obj = m_queue.front();
				m_queue.pop();
			}
		}
		return obj;
	}
	void Push(PtrT obj) {
		WXAutoLock al(m_lock);
		m_queue.push(obj);
	}
	void Flush() {
		WXAutoLock al(m_lock);
		while (!m_queue.empty()) {
			PtrT obj = m_queue.front();
			m_queue.pop();
			delete obj;
		}
	}
};

//线程类
typedef std::function<void()> WXTask;

class WXThread {
	std::thread *m_thread = nullptr;
	
	bool m_bUseTask = false;//异步任务
	ThreadSafeQueue<WXTask>m_queueTask;
protected:
	volatile bool m_bThreadStop = true;
public:
	virtual  void ThreadPrepare() {}//线程循环前的初始化
	virtual  void ThreadProcess() = 0;//线程循环函数,必须实现
	virtual  void ThreadPost() {}//线程循环结束后的退出处理
public:
	inline void RunTask(WXTask task)
	{
		m_queueTask.Push(task);//异步任务到线程
	}

	bool ThreadStart(bool bUseTask = false) {
		if (!m_bThreadStop) return false;//已经启动
		m_bThreadStop = false;
		m_bUseTask = bUseTask;
		m_thread = new std::thread([this] {
			ThreadPrepare();
			while (!m_bThreadStop) {
				if (m_bUseTask) { //异步任务
					WXTask task = nullptr;
					while (!m_queueTask.Empty()) {
						task = m_queueTask.Pop();//任务队列
						task(); //执行任务
					}
				}
				ThreadProcess();
			}
			ThreadPost();
		});
		return true;
	}
	void ThreadStop() {
		m_bThreadStop = true;
		if (m_thread) {
			m_thread->join();
			delete m_thread;
			m_thread = nullptr;
		}
	}
};

class WXAsync {
	bool m_bUseTask = false;//异步任务
	ThreadSafeQueue<WXTask>m_queueTask;
	std::future<int> m_sync;
protected:
	volatile bool m_bThreadStop = true;
public:
	virtual  int ThreadPrepare() {return 0;}//线程循环前的初始化
	virtual  int ThreadProcess() = 0;//线程循环函数,必须实现
	virtual  int ThreadPost() {return 0;}//线程循环结束后的退出处理
public:
	inline void RunTask(WXTask task)
	{
		m_queueTask.Push(task);//异步任务到线程
	}

	int ThreadStart(bool bUseTask = false) {
		if (!m_bThreadStop) return false;//已经启动
		m_bThreadStop = false;
		m_bUseTask = bUseTask;
		m_sync = std::async(std::launch::async, [this] {
			int ret = ThreadPrepare();
			if (ret < 0){
				return ret;
			}

			while (!m_bThreadStop) {
				if (m_bUseTask) { //异步任务
					WXTask task = nullptr;
					while (!m_queueTask.Empty()) {
						task = m_queueTask.Pop();//任务队列
						task(); //执行任务
					}
				}
				ret = ThreadProcess();
				if (ret < 0){
					return ret;
				}
			}
			ret = ThreadPost();
			if (ret < 0){
				return ret;
			}

			return 0;
		});

		return m_sync.get();
	}
	void ThreadStop() {
		m_bThreadStop = true;
	}
};

//日志类
class WXLogInstance{
	WXLocker m_mutex;
	WXString m_strFileName;
	std::ofstream m_fout;
public:
	WXLogInstance() {}

	virtual ~WXLogInstance() {
		Close();
	}

	void Close() {
		WXAutoLock al(m_mutex);
		if (m_fout.is_open()) {
			m_fout.close();
		}
	}

#ifdef _WIN32
	bool Open(const wchar_t* strDir, const wchar_t* strFileName, const wchar_t* strVersion, const wchar_t* strFileExt) {
		WXAutoLock al(m_mutex);
		Close();

		m_strFileName.Format(L"%ws%ws.%ws.%ws", strDir, strFileName,strVersion,strFileExt);

		struct stat st;
		int ret = stat(m_strFileName.c_str(), &st);
		if (ret == 0 && st.st_size > 1 * 1000 * 1000) {
			int rename2 = 1;
			while (1) {
				WXString wxstr;
				wxstr.Format(L"%ws%d-%ws.%ws.%ws", strDir, rename2, strFileName, strVersion, strFileExt);

				struct stat st2;
				int ret2 = stat(wxstr.c_str(), &st2);
				if (ret2 == 0) {
					rename2++;
					continue;
				}else {
					RENAME(m_strFileName.c_str(), wxstr.c_str()); //
					break;
				}
			}
		}

		m_fout.open(m_strFileName.str(), std::ios::app | std::ios::binary);
		if (m_fout.is_open()) {
			uint8_t headText[2] = { 0xff, 0xfe };
			m_fout.write((const char*)headText, 2);
			return true;
		}
		return m_fout.is_open();
	}

	bool Open(const wchar_t* strFileName) {
		WXAutoLock al(m_mutex);
		Close();

		m_strFileName =  strFileName;

		BOOL bExist = FALSE;
		struct stat st;
		int ret = stat(m_strFileName.c_str(), &st);
		if (ret == 0) {
			bExist = TRUE;
		}
		if (ret == 0 && st.st_size > 1 * 1000 * 1000) {
			int rename2 = 1;
			while (1) {
				WXString wxstr;
				wxstr.Format(L"%ws.%d", strFileName, rename2);

				struct stat st2;
				int ret2 = stat(wxstr.c_str(), &st2);
				if (ret2 == 0) {
					rename2++;
					continue;
				}
				else {
					bExist = FALSE;
					RENAME(m_strFileName.c_str(), wxstr.c_str()); //
					break;
				}
			}
		}

		m_fout.open(m_strFileName.str(), std::ios::app | std::ios::out | std::ios::binary);
		if (m_fout.is_open()) {
			if (!bExist) {
				uint8_t headText[2] = { 0xff, 0xfe };
				m_fout.write((const char*)headText, 2);
			}
			return true;
		}
		return m_fout.is_open();
	}


#else
	bool Open(const char* strFileName) {
		WXAutoLock al(m_mutex);
		Close();
		m_strFileName.Format(strFileName);
		m_fout.open(m_strFileName.c_str(), std::ios::app | std::ios::binary);
		return m_fout.is_open();
	}

#endif
    void Write(const wchar_t* wszMsg) {
        WXAutoLock al(m_mutex);
        if (m_fout.is_open()) {
            m_fout.write((const char*)wszMsg, sizeof(wchar_t)*wcslen(wszMsg));
            m_fout.flush();
        }
    }
    void Write(const char* szMsg) {
		WXAutoLock al(m_mutex);
		if (m_fout.is_open()) {
			m_fout.write(szMsg, sizeof(char)*strlen(szMsg));
		}
	}

	void Write(WXString strMsg) {
		WXAutoLock al(m_mutex);
		Write(strMsg.str());
	}
};

//单向队列
class WXFifo {
public:
	volatile int m_bEnable = 1;//是否可写
	WXDataBuffer m_dataBuffer;
	int64_t m_nTotalSize = 192000;
	int64_t m_nPosRead = 0;//读位置
	int64_t m_nPosWrite = 0;//写位置
	uint8_t m_last = 0;
	WXLocker m_mutex;
public:
	inline int64_t Size() { 
		WXAutoLock al(m_mutex);
		return m_nPosWrite - m_nPosRead; 
	}
public:
	WXFifo(int totolsize = 192000) {
		WXAutoLock al(m_mutex);
		m_dataBuffer.Init(nullptr, totolsize);
		m_nTotalSize = totolsize;
	}

	void Init(int totolsize = 192000) {
		WXAutoLock al(m_mutex);
		m_dataBuffer.Init(nullptr, totolsize);
		m_nTotalSize = totolsize;
	}

	void Reset() {
		WXAutoLock al(m_mutex);
		m_nPosRead = 0;
		m_nPosWrite = 0;
		memset(m_dataBuffer.m_pBuf, 0, m_dataBuffer.m_iBufSize);
	}

	virtual ~WXFifo() {
		WXAutoLock al(m_mutex);
		m_nPosRead = 0;
		m_nPosWrite = 0;
	}

	void Write(uint8_t *pBuf, int nSize) { //写数据
		WXAutoLock al(m_mutex);
		if (m_bEnable) {
			int64_t newSize = m_nPosWrite - m_nPosRead + nSize;//可写区域
			if (newSize < m_nTotalSize) { //数据区可写
				int64_t posWirte = m_nPosWrite % m_nTotalSize;//实际写入位置
				int64_t posLeft = m_nTotalSize - posWirte;//在实际位置上buffer还剩多少可以写入的空间
				if (posLeft >= nSize) { //数据区足够
					memcpy(m_dataBuffer.m_pBuf + posWirte, pBuf, nSize);
				}else {  //有部分数据需要写到RingBuffer的头部
					memcpy(m_dataBuffer.m_pBuf + posWirte, pBuf, posLeft);//写到RingBuffer尾部
					memcpy(m_dataBuffer.m_pBuf, pBuf + posLeft, nSize - posLeft);//写到RingBuffer头部
				}
				m_nPosWrite += nSize;//更新写位置
			}
		}
	}

	int Read(uint8_t *pBuf, int nSize) {//读数据
		WXAutoLock al(m_mutex);
		int64_t nowSize = m_nPosWrite - m_nPosRead;//可读区域
		if (nSize && nowSize >= nSize) { //数据足够读
			if (pBuf) {
				int64_t posRead = m_nPosRead % m_nTotalSize;//实际读取位置
				int64_t posLeft = m_nTotalSize - posRead;//在实际位置上buffer还剩多少可以写入的空间
				if (posLeft >= nSize) { //数据区足够
					memcpy(pBuf, m_dataBuffer.m_pBuf + posRead, (size_t)nSize);
				}
				else {  //有部分数据需要写到RingBuffer的头部
					memcpy(pBuf, m_dataBuffer.m_pBuf + posRead, (size_t)posLeft);//从尾部拷贝数据
					memcpy(pBuf + posLeft, m_dataBuffer.m_pBuf, (size_t)(nSize - posLeft));//从头部拷贝数据
				}
			}
			m_nPosRead += nSize;
			m_last = pBuf[nSize - 1];
			return nSize;
		}
		return 0;
	}

	int Read2(uint8_t *buf, int size) {//buf=nullptr Skip
		WXAutoLock al(m_mutex);
		memset(buf, m_last, size);
		int64_t nowSize = m_nPosWrite - m_nPosRead;//有效数据区
		if (nowSize >= size) {
			return Read(buf, size);
		}else if(nowSize > 0){
			int ret = Read(buf, nowSize);
			return ret;
		}		
		return 0;
	}
};


#endif
