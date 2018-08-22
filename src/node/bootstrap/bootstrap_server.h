//
// Created by daoful on 18-7-26.
//

#ifndef SRC_BOOTSTRAP_SERVER_H
#define SRC_BOOTSTRAP_SERVER_H




namespace germ
{
class node;
class tcp_socket;
class tcp_bootstrap_server : public std::enable_shared_from_this<tcp_bootstrap_server>
{
public:
    tcp_bootstrap_server (std::shared_ptr<germ::tcp_socket>, std::shared_ptr<germ::node>);
    ~tcp_bootstrap_server ();
    void receive ();
    void receive_header_action (boost::system::error_code const &, size_t);
    void receive_bulk_pull_action (boost::system::error_code const &, size_t, germ::message_header const &);
    void receive_bulk_pull_blocks_action (boost::system::error_code const &, size_t, germ::message_header const &);
    void receive_frontier_req_action (boost::system::error_code const &, size_t, germ::message_header const &);
    void receive_bulk_push_action ();
    void add_request (std::unique_ptr<germ::message>);
    void finish_request ();
    void run_next ();
    std::shared_ptr<std::vector<uint8_t>> receive_buffer;
    std::shared_ptr<germ::tcp_socket> socket;
    std::shared_ptr<germ::node> node;
    std::mutex mutex;
    std::queue<std::unique_ptr<germ::message>> requests;
};

}


#endif //SRC_BOOTSTRAP_SERVER_H
