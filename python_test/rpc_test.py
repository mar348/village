import json
import rpc_fun
import unittest


class rpcTest(unittest.TestCase):

    def setUp(self):
        self.rpc = rpc_fun.rpcs()

    def tearDown(selfself):
        pass

    def test_tx_count(self):
        print("\n--------------test_tx_count--------------------")
        tx_count = self.rpc.rpc_block_count()
        print("block_count: %s" % json.dumps(tx_count))

        count = tx_count['count']
        unchecked = tx_count['unchecked']
        self.assertGreaterEqual(count, 1, 'error: txs count < 1 ')
        self.assertGreaterEqual(unchecked, 0, 'error: txs unchecked < 0')

    def test_tx_count_type(self):
        print("\n--------------test_tx_count_type--------------------")
        tx_count_type = self.rpc.rpc_block_count_type()
        print("block_count_type: %s" % json.dumps(tx_count_type))

        send_count = tx_count_type['send']
        receive_count = tx_count_type['receive']
        open_count = int(tx_count_type['open'].encode('unicode-escape').decode('string-escape'))
        change_count = int(tx_count_type['change'].encode('unicode-escape').decode('string-escape'))
        self.assertGreaterEqual(send_count, 0, 'error: send tx count < 0')
        self.assertGreaterEqual(receive_count, 1, 'error: receive tx count < 1')
        self.assertEqual(open_count, 0, 'error: open tx count != 0')
        self.assertEqual(change_count, 0, 'error: change tx count != 0')

    def test_unchecked(self):
        print("\n--------------test_unchecked--------------------")
        unchecked = self.rpc.rpc_unchecked()
        print("unchecked: %s" % json.dumps(unchecked))

    def test_wallet_create(self):
        print("\n--------------test_wallet_create--------------------")
        wallet = self.rpc.rpc_wallet_create()
        print("wallet: %s" % wallet)
        self.assertEqual(len(wallet), 64, 'error: wallet length != 64')

    def test_wallet_add(self):
        print("\n--------------test_wallet_add--------------------")
        wallet = self.rpc.rpc_wallet_create()
        account = self.rpc.rpc_wallet_add(wallet)
        print("genesis account: %s" % account)

        # genesis account prv_key: 34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4
        self.assertEqual(account, 'xrb_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo', 'error: genesis account private key mismatch account.')

    def test_wallet_change_seed(self):
        print("\n--------------test_wallet_change_seed--------------------")
        wallet = self.rpc.rpc_wallet_create()
        account = self.rpc.rpc_wallet_add(wallet)
        print("genesis account: %s" % account)
        self.rpc.rpc_wallet_change_seed(wallet)
        accounts = self.rpc.rpc_accounts_create(wallet, 1)
        print("accounts: %s" % accounts)

    def test_account_create(self):
        print("\n--------------test_account_create--------------------")
        wallet = self.rpc.rpc_wallet_create()
        print("wallet: %s" % wallet)
        account = self.rpc.rpc_wallet_add(wallet)
        self.rpc.rpc_accounts_create(wallet)
        accounts = self.rpc.rpc_account_list(wallet)
        print("accounts: %s" % accounts)

    def test_wallet_balances(self):
        print("\n--------------test_wallet_balances--------------------")
        wallet = self.rpc.rpc_wallet_create()
        account = self.rpc.rpc_wallet_add(wallet)
        print("genesis account: %s" % account)
        balances = self.rpc.rpc_wallet_balances(wallet)
        print("balances: %s" % balances)

    def test_account_balance(self):
        print("\n--------------test_account_balance--------------------")
        wallet = self.rpc.rpc_wallet_create()
        account = self.rpc.rpc_wallet_add(wallet)
        print("genesis account: %s" % account)
        balances = self.rpc.rpc_account_balance(account)
        balances = balances['balance'].encode('unicode-escape').decode('string-escape')
        print("balances: %s" % balances)
        # self.assertEqual(balances, "340282366920938463463374607431767511455", 'error: genesis balance not equal 340282366920938463463374607431767511455')

    def test_send_receive(self):
        print("\n--------------test_send_receive--------------------")
        wallet = self.rpc.rpc_wallet_create()
        print("wallet: %s" % wallet)
        account = self.rpc.rpc_wallet_add(wallet)
        self.rpc.rpc_accounts_create(wallet, 1)
        accounts = self.rpc.rpc_account_list(wallet)
        account1 = accounts['accounts'][0]
        account2 = accounts['accounts'][1]
        print("account1: %s" % account1)
        print("account2: %s" % account2)
        send_block = self.rpc.rpc_send(wallet, account2, account1, 100000000000)
        print("send_block: %s" % send_block)
        recv_block = self.rpc.rpc_receive(wallet, account1, send_block)
        print("recv_block: %s" % recv_block)













if __name__ == '__main__':
    test = unittest.TestLoader().loadTestsFromTestCase(rpcTest)

    suite = unittest.TestSuite()
    suite.addTest(test)

    log_name = 'rpc_test.txt'
    with open(log_name, 'w+') as f:
        result = unittest.TextTestRunner(stream=f, verbosity=2).run(suite)



    print("\n----------------Tests result-----------------------")
    print("testRun: %s" % result.testsRun)
    print("failures: %s" % result.failures)
    print("errors: %s" % result.errors)
    print("skipped: %s" % result.skipped)