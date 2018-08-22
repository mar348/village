//
// Created by daoful on 18-8-1.
//

#include <src/lib/general_account.h>

germ::general_account::general_account()
:account(0),
 deposit(0)
{

}

germ::general_account::general_account(germ::account const &account_r, germ::amount const &deposit_r)
:account(account_r),
 deposit(deposit_r)
{
}
