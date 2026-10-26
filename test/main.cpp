#include <iostream>
#include <string>
#include <vector>
#include <future>       // 用于 std::async 和 std::future
#include <mutex>        // 用于保护 std::cout 输出
#include "httplib.h"
#include "json.hpp"

using json = nlohmann::json;
std::mutex print_mutex; // 防止多线程打印时文字交错

// ==========================================
// 1. 模拟你的“门面库”真实接口
// ==========================================
namespace FacadeAPI {
    void get_rsrp_data(long start_time, long end_time, int cell_id) {
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << "[底层执行] 获取RSRP数据: Start=" << start_time 
                  << ", End=" << end_time << ", Cell=" << cell_id << "\n";
    }
    void filter_data(const std::string& condition) {
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << "[底层执行] 过滤数据, 条件: " << condition << "\n";
    }
}

// ==========================================
// 2. 异步 AI 解析与执行引擎
// ==========================================
class AIScriptEngine {
private:
    std::string host;
    int port;
    std::string system_prompt;

public:
    AIScriptEngine(const std::string& h, int p) : host(h), port(p) {
        system_prompt = R"(你是一个空口数据分析助手。
请根据用户的需求，生成一个JSON格式的脚本，按步骤调用API。
只输出纯JSON字符串，不要输出任何 markdown 代码块标记，也不要输出任何解释性文字。

可用 API 列表：
1. get_rsrp_data(start_time, end_time, cell_id)
2. filter_data(condition)

JSON 输出格式参考：
{
  "steps": [
    {"action": "get_rsrp_data", "params": {"start_time": 1696000000, "end_time": 1696003600, "cell_id": 123}},
    {"action": "filter_data", "params": {"condition": "rsrp < -110"}}
  ]
})";
    }

    //核心改造1：将请求改为异步，返回 future
    std::future<std::string> async_generate_script(const std::string& user_input) {
        return std::async(std::launch::async, [this, user_input]() {
            // 最多重试 3 次
            for (int retry = 0; retry < 3; ++retry) {
                httplib::Client local_cli(host, port);
                local_cli.set_read_timeout(60, 0); 
                local_cli.set_connection_timeout(10, 0); // 设置连接超时

                json payload = {
                    {"model", "qwen"},
                    {"messages", {
                        {{"role", "system"}, {"content", system_prompt}},
                        {{"role", "user"}, {"content", user_input}}
                    }},
                    {"temperature", 0.1}
                };

                auto res = local_cli.Post("/v1/chat/completions", payload.dump(), "application/json");
                
                if (res && res->status == 200) {
                    json response_json = json::parse(res->body);
                    std::string content = response_json["choices"][0]["message"]["content"];
                    
                    size_t start = content.find("{");
                    size_t end = content.rfind("}");
                    if (start != std::string::npos && end != std::string::npos) {
                        return content.substr(start, end - start + 1);
                    }
                    return content;
                } else {
                    // 打印详细的网络错误
                    if (!res) {
                        std::cerr << "[重试 " << retry+1 << "/3] HTTP连接错误码: " 
                                  << static_cast<int>(res.error()) << std::endl;
                    } else {
                        std::cerr << "[重试 " << retry+1 << "/3] HTTP状态码错误: " 
                                  << res->status << std::endl;
                    }
                    
                    if (retry < 2) {
                        std::this_thread::sleep_for(std::chrono::seconds(2)); // 等2秒再重试
                    }
                }
            }
            throw std::runtime_error("网络请求彻底失败，无法连接 llama-server");
        });
    }

    void execute_script(const std::string& json_str) {
        try {
            json script = json::parse(json_str);
            for (const auto& step : script["steps"]) {
                std::string action = step["action"];
                auto params = step["params"];
                
                if (action == "get_rsrp_data") {
                    FacadeAPI::get_rsrp_data(params["start_time"].get<long>(), params["end_time"].get<long>(), params["cell_id"].get<int>());
                } else if (action == "filter_data") {
                    FacadeAPI::filter_data(params["condition"].get<std::string>());
                }
            }
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(print_mutex);
            std::cerr << "!! 脚本执行失败: " << e.what() << "\n";
        }
    }
};

// ==========================================
// 3. 主函数：模拟并发发起多个分析任务
// ==========================================
int main() {
    AIScriptEngine engine("127.0.0.1", 8080);

    // 模拟用户同时提出了 3 个分析需求
    std::vector<std::string> user_queries = {
        "查一下昨天小区123的RSRP数据，并过滤出小于-110的数据",
        "查一下今天小区456的RSRP数据，不过滤",
        "帮我查一下小区789的RSRP数据，过滤小于-100的数据"
    };

    std::vector<std::future<std::string>> futures;

    std::cout << "--- 1. 开始并发向 AI 发送请求 (非阻塞) ---\n";
    
    // 步骤 A：一口气把请求全发出去，主线程不等待
    for (const auto& query : user_queries) {
        std::cout << "发起请求: " << query << "\n";
        futures.push_back(engine.async_generate_script(query));
    }

    std::cout << "\n主线程继续做其他事情... (比如刷新UI、处理其他业务)\n\n";

    // 步骤 B：在这里统一等待结果（或者你可以把结果放到回调函数里）
    for (size_t i = 0; i < futures.size(); ++i) {
        try {
            std::string json_script = futures[i].get();
            
            // 用大括号 {} 限制锁的作用域
            {
                std::lock_guard<std::mutex> lock(print_mutex);
                std::cout << "\n================ 任务 " << i+1 << " 完成 ================\n";
                std::cout << "AI 生成的 JSON:\n" << json_script << "\n";
                std::cout << "--- 开始执行底层调用 ---\n";
            } // <--- 离开大括号，锁自动释放！

            // 此时主线程没有锁，execute_script 里的函数可以安全地拿锁
            engine.execute_script(json_script); 
            
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(print_mutex);
            std::cerr << "任务 " << i+1 << " 报错: " << e.what() << "\n";
        }
    }

    std::cout << "\n所有任务执行完毕。\n";
    return 0;
}