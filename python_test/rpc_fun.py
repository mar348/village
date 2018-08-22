import requests
import json


uri = 'http://localhost:55000'

test_genesis_priv_key = '34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4'
account_key = '74F2B37AAD20F4A260F0A5B3CB3D7FB51673212263E58A380BC10474BB039CEE'

class rpcs:

    def make_requests(self, uri, **args):
        res = requests.post(url=uri, data=json.dumps(args))
 #       print res.text
        return res.json()

    def rpc_block_count(self):
        count = self.make_requests(uri, action='block_count')
        return count

    def rpc_block_count_type(self):
        count_types = self.make_requests(uri,action='block_count_type')
        return count_types

    def rpc_unchecked(self, count=-1):
        blocks = self.make_requests(uri,action='unchecked', count=count)
        return blocks


    # about wallet commands

    def rpc_wallet_create(self):
        wallet = self.make_requests(uri,action='wallet_create')['wallet']
        return wallet

    def rpc_wallet_add(self,wallet,key=test_genesis_priv_key):
        account = self.make_requests(uri,action='wallet_add', wallet=wallet, key=key)['account']
        return account

    def rpc_wallet_change_seed(self, wallet, seed=account_key):
        self.make_requests(uri, action='wallet_change_seed', wallet=wallet, seed=seed)

    def rpc_accounts_create(self,wallet, count=1):
        accounts = self.make_requests(uri, action='accounts_create', wallet=wallet, count=count)
        return accounts

    def rpc_account_list(self, wallet):
        accounts = self.make_requests(uri, action='account_list', wallet=wallet)

    def rpc_wallet_balances(self,wallet):
        accounts_balances = self.make_requests(uri,action='wallet_balances',wallet=wallet)
        return accounts_balances


    # about account commands

    def rpc_account_balance(self,account):
        balance_info = self.make_requests(uri,action='account_balance', account=account)
        return balance_info

    def rpc_account_block_count(self,account):
        account_count = self.make_requests(uri,action='account_block_count', account=account)
        return account_count

    def rpc_account_info(self, account, pending=True):
        account_info = self.make_requests(uri,action='account_info', account=account, pending=pending)
        return account_info

    def rpc_account_get(self, wallet, key):
        account = self.make_requests(uri,action='account_get', wallet=wallet, key=key)
        return account

    def rpc_account_history(self, account, count):
        history = self.make_requests(uri,action='account_history', account=account, count=count)
        return history

    def rpc_account_list(self, wallet):
        accounts = self.make_requests(uri,action='account_list', wallet=wallet)
        return accounts

    def rpc_account_key(self, account):
        pub_key = self.make_requests(uri,action='account_key', account=account)['key']
        return pub_key

    def rpc_accounts_balances(self, accounts):
        balances = self.make_requests(uri,action='accounts_balances', accounts=accounts)
        return balances

    def rpc_accounts_frontiers(self, accounts):
        frontiers = self.make_requests(uri,action='accounts_frontiers', accounts=accounts)
        return frontiers

    def rpc_accounts_pending(self, accounts, count, source=True):
        pendings = self.make_requests(uri,action='accounts_pending', count=count, source=source)
        return pendings


    # about block commands

    def rpc_chain(self, block,count):
        chain =self.make_requests(uri,action='chain', block=block, count=count)
        return chain

    def rpc_block_account(self, hash):
        account = self.make_requests(uri,action='block_account', hash=hash)['account']
        return account

    def rpc_block_confirm(self, hash):
        self.make_requests(uri,action='block_confirm', hahs=hash)

    def rpc_deterministic_key(self, seed, index):
        keys = self.make_requests(uri,action='deterministic_key', seed=seed, index=index)
        prv_key = keys['private']
        pub_key = keys['public']
        account = keys['account']
        return prv_key,pub_key,account

    def rpc_frontiers(self, accounts, account, count):
        frontiers = self.make_requests(uri,action='frontiers', account=account, count=count)['frontiers']
        return frontiers

    def rpc_frontier_count(self):
        count = self.make_requests(uri,action='frontier_count')['count']
        return count

    def rpc_key_create(self):
        keypair = self.make_requests(uri,action='key_create')
        prv_key = keypair['private']
        pub_key = keypair['public']
        account = keypair['account']

    # create a send transaction
    def rpc_send_create(self, type, wallet, account, destination, balance, amount, previous):
        send_block = self.make_requests(uri,action='block_create', type=type, wallet=wallet, account=account, destination=destination, balance=balance, amount=amount, previous=previous)
        return send_block

    def rpc_receive_create(self, type, wallet, account, source, previous):
        receive_block = self.make_requests(uri,action='block_create', type=type, wallet=wallet, account=account, source=source, previous=previous)
        return receive_block

    def rpc_send(self, wallet, source, destination, amount):
        block = self.make_requests(uri,action='send', wallet=wallet, source=source, destination=destination, amount=amount)['block']
        return block

    def rpc_receive(self, wallet, account, block):
        block_r = self.make_requests(uri,action='receive', wallet=wallet, account=account, block=block)['block']
        return block_r