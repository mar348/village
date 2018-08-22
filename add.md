# modify


### 1. src/lib/blocks.cpp:

+ germ::deserialize_block_json    
----send_block    
----receive_block    
----open_block    
----change_block    

+ germ::deserialize_block         
----send_block    
----receive_block    
----open_block   
----change_block    


### 2. src/rpc.cpp

+ germ::rpc_handler::block_create () 
----send    
----receive     
----vote    
----open    
----line 1360, epoch hash = 0   


### 3. src/blockstore.cpp      

+ germ::block_store::block_database    
----change -- > vote    


+ germ::block_store::block_get_raw  
----change --> vote   


### 4. src/ledger/cpp    
  
+ germ::ledger::block_destination    


### 5. src/node/bootstrap/bulk_pull_client.cpp   
----send_block --> send_tx    
----receive_block --> receive_tx     
----change_block --> vote_tx     
----open_block --> open_tx     

### 5. src/node/bootstrap/bulk_push_server.cpp   
----send_block --> send_tx    
----receive_block --> receive_tx     
----change_block --> vote_tx     
----open_block --> open_tx     


### 6. src/node/node.cpp
+ node::node () : epoch_store
application_path_a / "epoch.ldb"      


### 7. src/common.cpp    
+ test_genesis_data    

"tx_info": {               
        "value":"0x00",        
        "data":"0x88888880988888888888888888888888888888099999999877",    
        "gas":"32000",    
        "gasprice":"100",     
        "signature":"0000914373A2F0CA1296475BAEE40500A7F0A7AD72A5A80C81D7FAB7F6C802B2CC7DB50F5DD0FB25B2EF11761FA7344A158DD5A700B21BD47DE5BD0F63153A02"    
    },     
    "epoch":"0000000000000000000000000000000000000000000000000000000000000000",          

+ genesis::genesis ()    
----germ::open_block --> germ::open_tx   

    
    
            
    
    
   

  
