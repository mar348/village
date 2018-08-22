//
// Created by daoful on 18-7-23.
//

#include <src/lib/election.h>


//germ::election_tally::election_tally(germ::amount const &weight_r, germ::account const &account_r)
//{
//    weight = weight_r;
//    account = account_r;
//}

germ::account_election::account_election()
{
        
}

germ::account_election::~account_election()
{

}

bool germ::account_election::elect_candidate()
{
    auto result(false);



    
    return result;
}


std::shared_ptr<germ::account_election> germ::account_election::shared()
{
    return shared_from_this();
}


