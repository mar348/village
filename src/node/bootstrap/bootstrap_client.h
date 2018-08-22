//
// Created by daoful on 18-7-26.
//

#ifndef SRC_BOOTSTRAP_CLIENT_H
#define SRC_BOOTSTRAP_CLIENT_H




namespace germ
{
class node;
class tcp_socket;
class tcp_bootstrap_attempt;
class tcp_bootstrap_client : public std::enable_shared_from_this<tcp_bootstrap_client>
{
public:
    tcp_bootstrap_client (std::shared_ptr<germ::node>, std::shared_ptr<germ::tcp_bootstrap_attempt>, germ::tcp_endpoint const &);
    ~tcp_bootstrap_client ();
    void run ();
    std::shared_ptr<germ::tcp_bootstrap_client> shared ();
    void stop (bool force);
    double block_rate () const;
    double elapsed_seconds () const;
    std::shared_ptr<germ::node> node;
    std::shared_ptr<germ::tcp_bootstrap_attempt> attempt;
    std::shared_ptr<germ::tcp_socket> socket;
    std::shared_ptr<std::vector<uint8_t>> receive_buffer;
    germ::tcp_endpoint endpoint;
    std::chrono::steady_clock::time_point start_time;
    std::atomic<uint64_t> block_count;
    std::atomic<bool> pending_stop;
    std::atomic<bool> hard_stop;
};

}


#endif //SRC_BOOTSTRAP_CLIENT_H
