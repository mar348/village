//
// Created by daoful on 18-7-26.
//

#ifndef SRC_BOOTSTRAP_INITIATOR_H
#define SRC_BOOTSTRAP_INITIATOR_H



namespace germ
{
class node;
class tcp_bootstrap_attempt;
class tcp_bootstrap_initiator : public std::enable_shared_from_this<tcp_bootstrap_initiator>
{
public:
    tcp_bootstrap_initiator (germ::node &);
    ~tcp_bootstrap_initiator ();
    void bootstrap (germ::endpoint const &, bool add_to_peers = true);
    void bootstrap ();
    void run_bootstrap ();
    void notify_listeners (bool);
    void add_observer (std::function<void(bool)> const &);
    bool in_progress ();
    std::shared_ptr<germ::tcp_bootstrap_attempt> current_attempt ();
    void stop ();

private:
    germ::node & node;
    std::shared_ptr<germ::tcp_bootstrap_attempt> attempt;
    bool stopped;
    std::mutex mutex;
    std::condition_variable condition;
    std::vector<std::function<void(bool)>> observers;
    std::thread thread;
};


}

#endif //SRC_BOOTSTRAP_INITIATOR_H
