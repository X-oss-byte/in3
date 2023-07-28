"""
Connects to Ethereum and fetches attested information from each chain.
"""

import in3

if __name__ == '__main__':

    client = in3.Client()
    try:
        print('\nEthereum Main Network')
        latest_block = client.eth.block_number()
        gas_price = client.eth.gas_price()
        print(f'Latest BN: {latest_block}\nGas Price: {gas_price} Wei')
    except in3.ClientException as e:
        print('Network might be unstable, try again later.\n Reason: ', e)

    goerli_client = in3.Client('goerli')
    try:
        print('\nEthereum Goerli Test Network')
        latest_block = goerli_client.eth.block_number()
        gas_price = goerli_client.eth.gas_price()
        print(f'Latest BN: {latest_block}\nGas Price: {gas_price} Wei')
    except in3.ClientException as e:
        print('Network might be unstable, try again later.\n Reason: ', e)

# Produces
"""
Ethereum Main Network
Latest BN: 9801135
Gas Price: 2000000000 Wei

Ethereum Goerli Test Network
Latest BN: 2460853
Gas Price: 4610612736 Wei
"""
