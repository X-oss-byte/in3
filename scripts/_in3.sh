#compdef in3

local -a subcmds args sig_in3 sig_erc20 sig_ms
subcmds=(
    'send: sends a transaction <signature> ...args'
    'call: calls a contract <signature> ...args'
    'abi_encode: encodes the arguments as described in the method signature using ABI-Encoding. <signature> ...args'
    'abi_decode: decodes the data based on the signature.. <signature> <data>'
    'btc_proofTarget: Whenever the client is not able to trust the changes of the target (which is the case if a block can... <target_dap> <verified_dap> <max_diff> <max_dap> <limit>'
    'getbestblockhash: Returns the hash of the best (tip) block in the longest blockchain'
    'getblock: Returns data of block for given block hash <hash> <verbosity>'
    'getblockcount: Returns the number of blocks in the longest blockchain'
    'getblockheader: Returns data of block header for given block hash <hash> <verbosity>'
    'getdifficulty: Returns the proof-of-work difficulty as a multiple of the minimum difficulty <blocknumber>'
    'getrawtransaction: Returns the raw transaction data <txid> <verbosity> <blockhash>'
    'eth_blockNumber: returns the number of the most recent block'
    'eth_call: calls a function of a contract (or simply executes the evm opcodes) and returns the result <tx> <block>'
    'eth_estimateGas: calculates the gas needed to execute a transaction <tx> <block>'
    'eth_getBalance: gets the balance of an account for a given block <account> <block>'
    'eth_getBlockByHash: Returns information about a block by hash <blockHash> <fullTx>'
    'eth_getBlockByNumber: returns information about a block by block number <blockNumber> <fullTx>'
    'eth_getBlockTransactionCountByHash: returns the number of transactions <blockHash>'
    'eth_getBlockTransactionCountByNumber: returns the number of transactions <blockNumber>'
    'eth_getCode: gets the code of a given contract <account> <block>'
    'eth_getLogs: searches for events matching the given criteria <filter>'
    'eth_getStorageAt: gets the storage value of a given key <account> <key> <block>'
    'eth_getTransactionByBlockHashAndIndex: returns the transaction data <blockHash> <index>'
    'eth_getTransactionByBlockNumberAndIndex: returns the transaction data <blockNumber> <index>'
    'eth_getTransactionByHash: returns the transaction data <txHash>'
    'eth_getTransactionCount: gets the nonce or number of transaction sent from this account at a given block <account> <block>'
    'eth_getTransactionReceipt: The Receipt of a Transaction <txHash>'
    'eth_getUncleCountByBlockHash: returns the number of uncles <blockHash>'
    'eth_getUncleCountByBlockNumber: returns the number of uncles <blockNumber>'
    'eth_sendRawTransaction: sends or broadcasts a prviously signed raw transaction <tx>'
    'eth_sendTransaction: signs and sends a Transaction <tx>'
    'eth_sendTransactionAndWait: signs and sends a Transaction, but then waits until the transaction receipt can be verified <tx>'
    'eth_sign: The sign method calculates an Ethereum specific signature with:  <account> <message>'
    'eth_signTransaction: Signs a transaction that can be submitted to the network at a later time using with eth_sendRawTrans... <tx>'
    'keccak: Returns Keccak-256 (not the standardized SHA3-256) of the given data'
    'net_version: the Network Version (currently 1)'
    'sha256: Returns sha-256 of the given data <data>'
    'web3_clientVersion: Returns the underlying client version'
    'web3_sha3: Returns Keccak-256 (not the standardized SHA3-256) of the given data <data>'
    'eth_accounts: returns a array of account-addresss the incubed client is able to sign with'
    'in3_abiDecode: based on the  <signature> <data>'
    'in3_abiEncode: based on the  <signature> <params>'
    'in3_addRawKey: adds a raw private key as signer, which allows signing transactions <pk>'
    'in3_cacheClear: clears the incubed cache (usually found in the '
    'in3_checksumAddress: Will convert an upper or lowercase Ethereum address to a checksum address <address> <useChainId>'
    'in3_config: changes the configuration of a client <config>'
    'in3_decryptKey: decrypts a JSON Keystore file as defined in the  <key> <passphrase>'
    'in3_ecrecover: extracts the public key and address from signature <msg> <sig> <sigtype>'
    'in3_ens: resolves a ens-name <name> <field>'
    'in3_fromWei: converts a given uint (also as hex) with a wei-value into a specified unit <value> <unit> <digits>'
    'in3_nodeList: fetches and verifies the nodeList from a node <limit> <seed> <addresses>'
    'in3_pk2address: extracts the address from a private key <pk>'
    'in3_pk2public: extracts the public key from a private key <pk>'
    'in3_prepareTx: prepares a Transaction by filling the unspecified values and returens the unsigned raw Transaction <tx>'
    'in3_sign: requests a signed blockhash from the node <blocks>'
    'in3_signData: signs the given data <msg> <account> <msgType>'
    'in3_signTx: signs the given raw Tx (as prepared by in3_prepareTx ) <tx> <from>'
    'in3_toWei: converts the given value into wei <value> <unit>'
    'in3_whitelist: Returns whitelisted in3-nodes addresses <address>'
    'ipfs_get: Fetches the data for a requested ipfs-hash <ipfshash> <encoding>'
    'ipfs_put: Stores ipfs-content to the ipfs network <data> <encoding>'
    'zksync_account_address: returns the address of the account used'
    'zksync_account_info: returns account_info from the server <address>'
    'zksync_aggregate_pubkey: calculate the public key based on multiple public keys signing together using schnorr musig signatur... <pubkeys>'
    'zksync_contract_address: returns the contract address'
    'zksync_deposit: sends a deposit-transaction and returns the opId, which can be used to tradck progress <amount> <token> <approveDepositAmountForERC20> <account>'
    'zksync_emergency_withdraw: withdraws all tokens for the specified token as a onchain-transaction <token>'
    'zksync_ethop_info: returns the state or receipt of the the PriorityOperation <opId>'
    'zksync_get_token_price: returns current token-price <token>'
    'zksync_get_tx_fee: calculates the fees for a transaction <txType> <address> <token>'
    'zksync_pubkey: returns the current packed PubKey based on the config set'
    'zksync_pubkeyhash: returns the current PubKeyHash based on the configuration set <pubKey>'
    'zksync_set_key: sets the signerkey based on the current pk or as configured in the config <token>'
    'zksync_sign: returns the schnorr musig signature based on the current config <message>'
    'zksync_sync_key: returns private key used for signing zksync-transactions'
    'zksync_tokens: returns the list of all available tokens'
    'zksync_transfer: sends a zksync-transaction and returns data including the transactionHash <to> <amount> <token> <account>'
    'zksync_tx_info: returns the state or receipt of the the zksync-tx <tx>'
    'zksync_verify: returns 0 or 1 depending on the successfull verification of the signature <message> <signature>'
    'zksync_withdraw: withdraws the amount to the given `ethAddress` for the given token <ethAddress> <amount> <token> <account>'
    )

args=(
 '-c[chain]:chain id:(mainnet goerli ewc ipfs btc local)'
 '-st[the type of the signature data]:st:(eth_sign raw hash)'
 '-p[the Verification level]:p:(none standard full)'
 '-pwd[password to unlock the key]:pwd:()'
 '-np[short for -p none]'
 '-ns[short for no stats, which does count this request in the public stats]'
 '-eth[onverts the result (as wei) to ether]'
 '-l[replaces "latest" with latest BlockNumber - the number of blocks given]:latest:(1 2 3 4 5 6 7 8 9 10)'
 '-s[number of signatures to use when verifying]:sigs:(1 2 3 4 5 6 7 8 9 10)'
 '-port[if specified it will run as http-server listening to the given port]:port:(8545)'
 '-am[Allowed Methods when used with -port as comma seperated list of methods]:allowed_methods:()'
 '-b[the blocknumber to use when making calls]:b:(latest earliest 0x)'
 '-to[the target address of the call or send]:to:(0x)'
 '-d[the data for a transaction. This can be a filepath, a 0x-hexvalue or - for stdin]:date:()'
 '-gas[the gas limit to use when sending transactions]:gas:(21000 100000 250000 500000 1000000 2000000)'
 '-gas_price[the gas price to use within a tx]:gas_price:()'
 '-pk[the private key as raw or path to the keystorefile]:pk:()'
 '-k[the private key to sign request for incetives payments]:k:()'
 '-help[displays this help message]'
 '-tr[runs test request when showing in3_weights]'
 '-thr[runs test request including health-check when showing in3_weights]'
 '-ms[address of a imao multisig to redirect all tx through]:ms:()'
 '-version[displays the version]'
 '-debug[if given incubed will output debug information when executing]'
 '-value[the value to send when sending a transaction. (hexvalue or float/integer with the suffix eth]:value:(1.0eth)'
 '-w[instead returning the transaction, it will wait until the transaction is mined and return the transactionreceipt]'
 '-md[specifies the minimum Deposit of a node in order to be selected as a signer]'
 '-json[the result will be returned as json]'
 '-hex[the result will be returned as hex]'
 '-kin3[the response including in3-section is returned]'
 '-q[quiet no debug output]'
 '-os[only sign, do not send the raw Transaction]'
 '-ri[read response from stdin]'
 '-ro[write raw response to stdout]'
 '-a[max number of attempts before giving up (default 5)]:attempts:(1 2 3 4 5 6 7 8 9)'
 '-rc[number of request per try (default 1)]:requestCount:(1 2 3 4 5 6 7 8 9)'
 '-fi[read recorded request from file]:fi:()'
 '-fo[recorded request and write to file]:fo:()'
 '-nl[a comma seperated list of urls as address:url to be used as fixed nodelist]:nl:()'
 '-bn[a comma seperated list of urls as address:url to be used as boot nodes]:bn:()'
 '-zks[URL of the zksync-server]:zks:(https\://api.zksync.io/jsrpc http\://localhost\:3030)'
 '-zkss[zksync signatures to pass along when signing]:zkss:()'
 '-zka[zksync account to use]:zka:()'
 '-zkat[zksync account type]:zkat:(pk contract create2)'
 '-zsk[zksync signer seed - if not set this key will be derrived from account unless create2]:zsk:()'
 '-zc2[zksync create2 arguments in the form <creator>:<codehash>:<saltarg>. if set the account type is also changed to create2]:zc2:()'
 '-zms[public keys of a musig schnorr signatures to sign with]:zms:()'
 '-zmu[url for signing service matching the first remote public key]:zmu:(http\://localhost\:8080)'
 '-zvpm[method for calling to verify the proof in server mode]:zvpm:(iamo_zksync_verify_signatures)'
 '-zcpm[method for calling to create the proof]:zcpm:(iamo_zksync_create_signatures)'
 '-idk[iamo device key]:idk:()'
 '-imc[the master copy address to be used]:imc:()'
 '-if[iamo factory address]:if:()'

 ':method:{_describe command subcmds}'
 ':arg1:{_describe command sig_in3 -- sig_erc20 -- sig_ms}'
)

sig_in3=(
    'minDeposi()\:uint: minimal deposit'
    'adminKey()\:address: admin key'
    'nodeRegistryData()\:address:addres of the data contract'
    'supportedToken()\:address: supported token'
    'totalNodes()\:uint: number of nodes'
    'blockRegistry()\:address: BlockHashRegistry'
    'nodes(uint256)\:(string,uint256,uint64,uint192,uint64,address,bytes32): node data'
    'unregisteringNode(address):unregister a node'
    'updateNode(address,string,uint192,uint64,uint256): update nod properties'
    'transferOwnership(address,address): transfers ownership from signer to new owner',
    'registerNode(string,uint192,uint64,uint256): registers a Node'
    'snapshot(): creates a snapshot for the current block'
)
sig_erc20=(
    'balanceOf(address)\:uint:getting the balance of' 
    'name()\:string:token name' 
    'totalSupply()\:uint:total Balance'
    'transfer(address,uint256):transfer tokens'
    'transferFrom(address,address,uint256):transfer from <my> to <your> account <value> tokens'
    'approve(address,uint256):approve the amount for the given address'
    'allowance(address,address)\:uint: the approved amount'
)
sig_ms=(
    'getOwners()\:(address[]): multisig'
    'getMessageHash(bytes)\:bytes: gets the message hash of a transaction'
    'isOwner(address)\:bool:is owner'
    'signedMessages(bytes32)\:uint: number of signed messages'
    'approvedHashes(address,bytes32)\:uint:check if the hash was approved'
    'nonce()\:uint:the nonce of the multisig'
    'getModules()\:address[]:List of modules'
    'getTransactionHash(address,uint256,bytes,uint8,uint256,uint256,uint256,address,address,uint256)\:bytes32:calculates the transaction hash'
    'getThreshold()\:uint'
    'addOwnerWithThreshold(address,uint256):adds an owner with the given threshold'
    'changeThreshold(uint256): changes the threshold'
    'execTransaction(address,uint256,bytes,uint8,uint256,uint256,uint256,address,address,bytes): executes a transaction'
)

_arguments -C $args
