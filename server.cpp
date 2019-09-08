#include "httplib.h"
#include <boost/filesystem.hpp>
#include <iostream>
#include <sstream>
#include <fstream>
#include "compress.hpp"

#define SERVER_BASE_DIR "www"
#define SERVER_ADDR "0.0.0.0"
#define SERVER_PORT 9000
#define SERVER_BACKUP_DIR SERVER_BASE_DIR"/list/"

using namespace httplib;
namespace bf = boost::filesystem;

CompressStore cstor;

class CloudServer{
private:
	SSLServer srv;
public:
	CloudServer(const char* cert, const char* key): srv(cert, key){
		bf::path base_path(SERVER_BASE_DIR);
		//如果文件不存在
		if (!bf::exists(base_path)){
			//创建目录
			bf::create_directory(base_path);
		}
		bf::path list_path(SERVER_BACKUP_DIR);
		if (!bf::exists(list_path)){
			bf::create_directory(list_path);
		}
		

	}
	bool Start(){
		srv.set_base_dir(SERVER_BASE_DIR);
		srv.Get("/(list(/){0,1}){0,1}", GetFileList);
		srv.Get("/list/(.*)", GetFileData);
		srv.Put("/list/(.*)", PutFileData);
		srv.listen("SERVER_ADDR", SERVER_PORT);
		return true;
	}
private:
	static void PutFileData(const Request &req, httplib::Response &rsp)
	{
		std::cout << "backup file " << req.path << "\n";
		if (!req.has_header("Range")){
			rsp.status = 400;
			return;
		}

			std::string range = req.get_header_value("Range");
			int64_t range_start;
			if (RangeParse(range, range_start) == false){
				rsp.status = 400;
				return;
			
		}
			
			std::string real = SERVER_BASE_DIR + req.path;
			cstor.SetFileData(real, req.body, range_start);
			std::ofstream file(real, std::ios::binary | std::ios::trunc);
			if (!file.is_open()){
				std::cerr << "open file " << real << "error\n";
				rsp.status = 500;
				return;
			}
			file.seekp(range_start, std::ios::beg);
			file.write(&req.body[0], req.body.size());
			if (!file.good()){
				std::cerr << "file write body error\n";
				rsp.status = 500;
				return;
			}
			file.close();
		return;
	}

	static bool RangeParse(std::string &range, int64_t &start){

		//Range: bytes=start-end
		size_t pos1 = range.find("=");
		size_t pos2 = range.find("=");
		if (pos1 == std::string::npos || pos2 == std::string::npos){
			std::cerr << "range:[" << range << "] format error\n";
			return false;
		}
		std::stringstream rs; 
		rs << range.substr(pos1 + 1, pos2 - pos1 - 1);
		rs >> start;
		return true;
	}
	//文件列表信息获取,定义成静态就是避免了this指针
	static void GetFileList(const Request &req, httplib::Response &rsp){
		
		std::vector<std::string> list;
		cstor.GetFileList(list);

		std::string body;
		body = "<html><body><ol><hr />";



		for (auto i:list){
			bf::path path(i);
			std::string file = path.filename().string();
			std::string uri = "/list/" + file;
			body += "<h4><li>";
			body += "<a href='";
			body += uri;
			body += "'>";
			body += file;
			body += "</a>";
			body += "</li></h4>";
			//std::string file = item_begin->path().string();
			//std::cerr << "file:" << file << std::endl;
		}
		body += "<hr /></ol></body></html>";
		rsp.set_content(&body[0], "text/html");
		return;
	}
	//获取文件数据（文件下载）
	static void GetFileData(const Request &req, httplib::Response &rsp){
		std::string real = SERVER_BASE_DIR + req.path;
		std::string  body;
		cstor.GetFileData(real, body);
		//正文只能给一次，文件不能太大
		rsp.set_content(body, "text/plain"); //plain文件下载
	}


};

void thr_start(){
	cstor.LowHeatFileStore();
}
int main()
{
	std::thread thr(thr_start);
	thr.detach();
	CloudServer srv("./cert.pem","./key.pem");
	srv.Start();
	return 0;
}

