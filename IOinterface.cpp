#include "IOinterface.h"
#include "buffer2.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define HF_STDIN 0
#define HF_STDOUT 1

#include <string.h>
#include <errno.h>
#include <memory>
#include <sstream>
#include <MMUtil+.h>

#ifdef __OS2__
#define INCL_BASE
#include <os2.h>
typedef int socklen_t; // well OS/2 special...

#define APICHK(r, m) \
	if (r) \
		throw os_error(r, m);

#else
#include <sys/stat.h>
#include <fcntl.h>
#define soclose close
#define sock_errno() errno
#endif


using namespace std;

#include "buffer2.h"

// ********** internal types 

// general file services
struct FileServices
{	
	#ifdef __OS2__
	typedef HFILE handle_t;
	#else
	typedef int handle_t;
	#endif
	handle_t HF;
	FileServices();
	~FileServices();
	#ifdef __OS2__
	bool isPipe(const char* name);
	#endif
};

// general TCP/IP services
struct TcpipServices
{	bool IsServer;
	sockaddr_in Addr; 
	int Socket;
	TcpipServices() : Socket(-1) {}
	~TcpipServices();
	void Initialize();
	void Parse(const char* url);
	string ConnectString() const;
	static string IP2string(u_long ip);
};

// input interface classes
class FileInput : public IInput, protected FileServices
{public:
	FileInput(const char* src) : IInput(src) {}
	virtual void Initialize();
	virtual size_t ReadData(void* dst, size_t len);
};

class TcpipInput : public IInput, protected TcpipServices
{public:
	TcpipInput(const char* src) : IInput(src) { Parse(src); }
	virtual void Initialize();
	virtual size_t ReadData(void* dst, size_t len);
};

// output interface classes
class FileOutput : public IOutput, protected FileServices
{public:
	FileOutput(const char* dst) : IOutput(dst) {}
	virtual void Initialize();
	virtual size_t WriteData(const void* src, size_t len);
};

class TcpipOutput : public IOutput, protected TcpipServices
{
 public:
	TcpipOutput(const char* dst) : IOutput(dst) { Parse(dst); }
	virtual void Initialize();
	virtual size_t WriteData(const void* src, size_t len);
};


using namespace MM;


// generic file services

#ifdef __OS2__
FileServices::FileServices() : HF((HFILE)-1)
{}

FileServices::~FileServices()
{	if (HF != (HFILE)-1)
	{	APIRET rc = DosClose(HF);
		if (rc != NO_ERROR)
			lerr << "Internal error " << rc << " while closing file handle " << HF << endl;
	}
}

bool FileServices::isPipe(const char* name)
{	if (strnicmp(name, "\\PIPE\\", 6) == 0) // local pipe
		return true;
	if (strncmp(name, "\\\\", 2) != 0) // no remote name
		return false;
	const char* p = strchr(name+2, '\\');
	if (p != NULL && strnicmp(p+1, "PIPE\\", 5) == 0) // remote pipe
		return true;
	return false;
}

#else
// generic implementation
FileServices::FileServices() : HF(-1)
{}

FileServices::~FileServices()
{	if (HF != -1)
	{	if (close(HF) != 0)
			lerr << "Internal error " << errno << " while closing file handle " << HF << endl;
	}
}

#endif


// generic TCP/IP services
#if defined(__OS2__) || defined (_WIN32)
#define TCPIPPREFIX "tcpip:\\\\"
#else
#define TCPIPPREFIX "tcpip://"
#endif

void TcpipServices::Parse(const char* url)
{	const char* cp = strchr(url, ':');
	if (cp == NULL)
		throw syntax_error(stringf("The TCP/IP URL tcpip://%s does not contain a port number. tcpip://hostname:port expected.", url));
	memset(&Addr, 0, sizeof Addr);
	Addr.sin_family = AF_INET;
	// host
	string host(url, cp-url);
	if (cp == url)
	{	// host empty
		IsServer = true;
		Addr.sin_addr.s_addr = INADDR_ANY;
	} else if ((Addr.sin_addr.s_addr = ::inet_addr(host.c_str())) != INADDR_NONE)
	  // IPV4
		IsServer = false;
	 else
	{	// name lookup
		hostent* hp = ::gethostbyname(host.c_str());
		if (hp == NULL)
			throw os_error(h_errno, stringf("The hosname %s cannot be resolved.", host.c_str()));
		IsServer = false;
		Addr.sin_addr.s_addr = *(u_long*)hp->h_addr;
	}
	// port
	++cp;
	size_t len;
	if (sscanf(cp, "%hi%n", &Addr.sin_port, &len) == 1 && len == strlen(cp))
		// numeric port
		Addr.sin_port = ::htons(Addr.sin_port);
	 else
	{	// non-numeric notation
		servent* sp = ::getservbyname(cp, "TCP");
		if (sp == NULL)
			throw os_error(h_errno, stringf("The service %s is not listed in etc/services or does not belong to the TCP protocol.", cp));
		Addr.sin_port = sp->s_port;
	}
}

TcpipServices::~TcpipServices()
{	if (Socket != -1)
		soclose(Socket);
}

void TcpipServices::Initialize()
{	// create socket
	Socket = ::socket(PF_INET, SOCK_STREAM, 0);
	if (Socket == -1)
		throw os_error(sock_errno(), "Failed to create socket.");
	// connect socket
	if (IsServer)
	{	if (::bind(Socket, (sockaddr*)&Addr, sizeof Addr))
			throw os_error(sock_errno(), "Failed to bind "+ConnectString()+".");
		if (::listen(Socket, 0))
			throw os_error(sock_errno(), "Failed to listen on "+ConnectString()+".");
		socklen_t len = sizeof Addr;
		int new_sock = ::accept(Socket, (sockaddr*)&Addr, &len);
		if (new_sock == -1)
			throw os_error(sock_errno(), "Failed to accept connection on "+ConnectString()+".");
		soclose(Socket); // do not accept further connections
		Socket = new_sock;
	} else
	{	if (::connect(Socket, (sockaddr*)&Addr, sizeof Addr))
			throw os_error(sock_errno(), "Failed to connect to "+ConnectString()+".");
	}
}

string TcpipServices::ConnectString() const
{	ostringstream oss;
	oss << IP2string(Addr.sin_addr.s_addr) << ':' << ntohs(Addr.sin_port);
	return oss.str();
}

string TcpipServices::IP2string(u_long ip)
{	ip = ::ntohl(ip);
	return stringf("%d.%d.%d.%d", ip>>24, (ip>>16) & 0xff, (ip>>8) & 0xff, ip & 0xff);
}

// input interface functions

IInput* IInput::Factory(const char* src)
{	if (strncmp(src, TCPIPPREFIX, 8) == 0)
		return new TcpipInput(src+8);
	 else
		return new FileInput(src);
}

#ifdef __OS2__
void FileInput::Initialize()
{	if (strcmp(Src, "-") == 0)
		HF = HF_STDIN;
	 else if (isPipe(Src))
	{	// named pipe
		// first try to open an existing instance
		ULONG action;
		ULONG rc = DosOpenL((PUCHAR)Src, &HF, &action, 0, 0, OPEN_ACTION_FAIL_IF_NEW|OPEN_ACTION_OPEN_IF_EXISTS,
			OPEN_FLAGS_SEQUENTIAL|OPEN_SHARE_DENYREAD|OPEN_ACCESS_READONLY, NULL);
		if (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND)
		{	// try to create the pipe
			APICHK(DosCreateNPipe((PUCHAR)Src, &HF, NP_ACCESS_INBOUND, NP_WAIT|NP_TYPE_BYTE|1, 0, PipeSize, 0)
			 , stringf("Failed to create named inbound pipe %s.", Src));
			APICHK(DosConnectNPipe(HF), stringf("Failed to connect named inbound pipe %s.", Src));
		} else
			APICHK(rc, stringf("Failed to open named inbound pipe %s.", Src));
	} else
	{	// ordinary file
		ULONG action;
		APICHK(DosOpenL((PUCHAR)Src, &HF, &action, 0, 0, OPEN_ACTION_FAIL_IF_NEW|OPEN_ACTION_OPEN_IF_EXISTS,
		 EnableCache ? OPEN_FLAGS_SEQUENTIAL|OPEN_SHARE_DENYNONE|OPEN_ACCESS_READONLY : OPEN_FLAGS_NO_CACHE|OPEN_FLAGS_SEQUENTIAL|OPEN_SHARE_DENYNONE|OPEN_ACCESS_READONLY, NULL)
		 , stringf("Failed to open %s for input.", Src));
	}
}

size_t FileInput::ReadData(void* dst, size_t len)
{	ULONG rlen;
	APICHK(DosRead(HF, dst, len, &rlen), "Failed to read from input stream.");
	return rlen;
}

#else
// generic implementation
void FileInput::Initialize()
{	if (strcmp(Src, "-") == 0)
		HF = HF_STDIN;
	 else
	{	// ordinary file
		HF = open(Src, EnableCache ? O_RDONLY : O_RDONLY|O_SYNC);
		if (HF == -1) 
			throw os_error(errno, stringf("Failed to open %s for input.", Src));
	}
}

size_t FileInput::ReadData(void* dst, size_t len)
{	len = read(HF, dst, len);
	if (len == (size_t)-1)
		throw os_error(errno, "Failed to read from input stream.");
	return len;
}

#endif

void TcpipInput::Initialize()
{	TcpipServices::Initialize();
}

size_t TcpipInput::ReadData(void* dst, size_t len)
{	int r = ::recv(Socket, dst, len, 0);
	if (r == -1)
		throw os_error(sock_errno(), "Error while receiving data from "+ConnectString()+".");
	return r;
}


// output worker functions

IOutput* IOutput::Factory(const char* src)
{	if (strncmp(src, TCPIPPREFIX, 8) == 0)
		return new TcpipOutput(src+8);
	 else
		return new FileOutput(src);
}

#ifdef __OS2__
void FileOutput::Initialize()
{	if (strcmp(Dst, "-") == 0)
		HF = HF_STDOUT;
	 else if (isPipe(Dst))
	{	// named pipe
		// first try to open an existing instance
		ULONG action;
		ULONG rc = DosOpenL((PUCHAR)Dst, &HF, &action, 0, 0, OPEN_ACTION_FAIL_IF_NEW|OPEN_ACTION_OPEN_IF_EXISTS,
			OPEN_FLAGS_SEQUENTIAL|OPEN_SHARE_DENYWRITE|OPEN_ACCESS_WRITEONLY, NULL);
		if (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND)
		{	// try to create the pipe
			APICHK(DosCreateNPipe((PUCHAR)Dst, &HF, NP_ACCESS_OUTBOUND, NP_WAIT|NP_TYPE_BYTE|1, PipeSize, 0, 0)
			 , stringf("Failed to create outbound pipe %s.", Dst));
			APICHK(DosConnectNPipe(HF), stringf("Failed to connect outbound pipe %s.", Dst));
		} else
			APICHK(rc, stringf("Failed to open outbound pipe %s.", Dst));
	} else
	{	// ordinary file
		ULONG action;
		APICHK(DosOpenL((PUCHAR)Dst, &HF, &action, 0, 0, OPEN_ACTION_CREATE_IF_NEW|OPEN_ACTION_REPLACE_IF_EXISTS,
		 EnableCache ? OPEN_FLAGS_SEQUENTIAL|OPEN_SHARE_DENYWRITE|OPEN_ACCESS_WRITEONLY : OPEN_FLAGS_NO_CACHE|OPEN_FLAGS_SEQUENTIAL|OPEN_SHARE_DENYWRITE|OPEN_ACCESS_WRITEONLY, NULL)
		 , stringf("Failed to open %s for output.", Dst));
	}
}

size_t FileOutput::WriteData(const void* src, size_t len)
{	ULONG rlen;
	APICHK(DosWrite(HF, src, len, &rlen), "Failed to write to output stream.");
	return rlen;
}

#else
// generic implementation

void FileOutput::Initialize()
{	if (strcmp(Dst, "-") == 0)
		HF = HF_STDOUT;
	 else
	{	// ordinary file
		HF = open(Dst, EnableCache ? O_CREAT|O_TRUNC|O_WRONLY : O_CREAT|O_TRUNC|O_WRONLY|O_SYNC);
		if (HF == -1) 
			throw os_error(errno, stringf("Failed to open %s for output.", Dst));
	}
}

size_t FileOutput::WriteData(const void* src, size_t len)
{	len = write(HF, src, len);
	if (len == (size_t)-1)
		throw os_error(errno, "Failed to write to output stream.");
	return len;
}

#endif

void TcpipOutput::Initialize()
{	TcpipServices::Initialize();
}

size_t TcpipOutput::WriteData(const void* src, size_t len)
{	int r = ::send(Socket, (char*)src, len, 0);
	if (r == -1)
		throw os_error(sock_errno(), "Error while sending data to "+ConnectString()+".");
	return r;
}


