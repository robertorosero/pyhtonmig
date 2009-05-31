# This should be extended for each Python release.
# The product code must change whenever the name of the MSI file
# changes, and when new component codes are issued for existing
# components. See "Changing the Product Code". As we change the
# component codes with every build, we need a new product code
# each time. For intermediate (snapshot) releases, they are automatically
# generated. For official releases, we record the product codes,
# so people can refer to them.
product_codes = {
    '2.4.101': '{0e9b4d8e-6cda-446e-a208-7b92f3ddffa0}', # 2.4a1, released as a snapshot
    '2.4.102': '{1b998745-4901-4edb-bc52-213689e1b922}', # 2.4a2
    '2.4.103': '{33fc8bd2-1e8f-4add-a40a-ade2728d5942}', # 2.4a3
    '2.4.111': '{51a7e2a8-2025-4ef0-86ff-e6aab742d1fa}', # 2.4b1
    '2.4.112': '{4a5e7c1d-c659-4fe3-b8c9-7c65bd9c95a5}', # 2.4b2
    '2.4.121': '{75508821-a8e9-40a8-95bd-dbe6033ddbea}', # 2.4c1
    '2.4.122': '{83a9118b-4bdd-473b-afc3-bcb142feca9e}', # 2.4c2
    '2.4.150': '{82d9302e-f209-4805-b548-52087047483a}', # 2.4.0
    '2.4.1121':'{be027411-8e6b-4440-a29b-b07df0690230}', # 2.4.1c1
    '2.4.1122':'{02818752-48bf-4074-a281-7a4114c4f1b1}', # 2.4.1c2
    '2.4.1150':'{4d4f5346-7e4a-40b5-9387-fdb6181357fc}', # 2.4.1
    '2.4.2121':'{5ef9d6b6-df78-45d2-ab09-14786a3c5a99}', # 2.4.2c1
    '2.4.2150':'{b191e49c-ea23-43b2-b28a-14e0784069b8}', # 2.4.2
    '2.4.3121':'{f669ed4d-1dce-41c4-9617-d985397187a1}', # 2.4.3c1
    '2.4.3150':'{75e71add-042c-4f30-bfac-a9ec42351313}', # 2.4.3
    '2.4.4121':'{cd2862db-22a4-4688-8772-85407ea21550}', # 2.4.4c1
    '2.4.4150':'{60e2c8c9-6cf3-4b1a-9618-e304946c94e6}', # 2.4.4
    '2.5.101': '{bc14ce3e-5e72-4a64-ac1f-bf59a571898c}', # 2.5a1
    '2.5.102': '{5eed51c1-8e9d-4071-94c5-b40de5d49ba5}', # 2.5a2
    '2.5.103': '{73dcd966-ffec-415f-bb39-8342c1f47017}', # 2.5a3
    '2.5.111': '{c797ecf8-a8e6-4fec-bb99-526b65f28626}', # 2.5b1
    '2.5.112': '{32beb774-f625-439d-b587-7187487baf15}', # 2.5b2
    '2.5.113': '{89f23918-11cf-4f08-be13-b9b2e6463fd9}', # 2.5b3
    '2.5.121': '{8e9321bc-6b24-48a3-8fd4-c95f8e531e5f}', # 2.5c1
    '2.5.122': '{a6cd508d-9599-45da-a441-cbffa9f7e070}', # 2.5c2
    '2.5.150': '{0a2c5854-557e-48c8-835a-3b9f074bdcaa}', # 2.5.0
    '2.5.1121':'{0378b43e-6184-4c2f-be1a-4a367781cd54}', # 2.5.1c1
    '2.5.1150':'{31800004-6386-4999-a519-518f2d78d8f0}', # 2.5.1
    '2.5.2150':'{6304a7da-1132-4e91-a343-a296269eab8a}', # 2.5.2c1
    '2.5.2150':'{6b976adf-8ae8-434e-b282-a06c7f624d2f}', # 2.5.2
    '2.6.101': '{0ba82e1b-52fd-4e03-8610-a6c76238e8a8}', # 2.6a1
    '2.6.102': '{3b27e16c-56db-4570-a2d3-e9a26180c60b}', # 2.6a2
    '2.6.103': '{cd06a9c5-bde5-4bd7-9874-48933997122a}', # 2.6a3
    '2.6.104': '{dc6ed634-474a-4a50-a547-8de4b7491e53}', # 2.6a4
    '2.6.111': '{3f82079a-5bee-4c4a-8a41-8292389e24ae}', # 2.6b1
    '2.6.112': '{8a0e5970-f3e6-4737-9a2b-bc5ff0f15fb5}', # 2.6b2
    '2.6.113': '{df4f5c21-6fcc-4540-95de-85feba634e76}', # 2.6b3
    '2.6.121': '{bbd34464-ddeb-4028-99e5-f16c4a8fbdb3}', # 2.6c1
    '2.6.122': '{8f64787e-a023-4c60-bfee-25d3a3f592c6}', # 2.6c2
    '2.6.150': '{110eb5c4-e995-4cfb-ab80-a5f315bea9e8}', # 2.6.0
    '2.6.1150':'{9cc89170-000b-457d-91f1-53691f85b223}', # 2.6.1
    '2.6.2121':'{adac412b-b209-4c15-b6ab-dca1b6e47144}', # 2.6.2c1
    '2.6.2150':'{24aab420-4e30-4496-9739-3e216f3de6ae}', # 2.6.2
    '3.0.101': '{8554263a-3242-4857-9359-aa87bc2c58c2}', # 3.0a1
    '3.0.102': '{692d6e2c-f0ac-40b8-a133-7191aeeb67f9}', # 3.0a2
    '3.0.103': '{49cb2995-751a-4753-be7a-d0b1bb585e06}', # 3.0a3
    '3.0.104': '{87cb019e-19fd-4238-b1c7-85751437d646}', # 3.0a4
    '3.0.105': '{cf2659af-19ec-43d2-8c35-0f6a09439d42}', # 3.0a5
    '3.0.111': '{36c26f55-837d-45cf-848c-5f5c0fb47a28}', # 3.0b1
    '3.0.112': '{056a0fbc-c8fe-4c61-aade-c4411b70c998}', # 3.0b2
    '3.0.113': '{2b2e89a9-83af-43f9-b7d5-96e80c5a3f26}', # 3.0b3
    '3.0.114': '{e95c31af-69be-4dd7-96e6-e5fc85e660e6}', # 3.0b4
    '3.0.121': '{d0979c5e-cd3c-42ec-be4c-e294da793573}', # 3.0c1
    '3.0.122': '{f707b8e9-a257-4045-818e-4923fc20fbb6}', # 3.0c2
    '3.0.123': '{5e7208f1-8643-4ea2-ab5e-4644887112e3}', # 3.0c3
    '3.0.150': '{e0e56e21-55de-4f77-a109-1baa72348743}', # 3.0.0
    '3.0.1121':'{d35b1ea5-3d70-4872-bf7e-cd066a77a9c9}', # 3.0.1c1
    '3.0.1150':'{de2f2d9c-53e2-40ee-8209-74da63cb060e}', # 3.0.1
    '3.0.2121':'{cef79e7f-9809-49e2-afd2-e24148d7c855}', # 3.0.2c1
    '3.0.2150':'{0cf3b95a-8382-4607-9779-c36407ff362c}', # 3.0.2
    '3.1.101': '{c423eada-c498-4d51-9eb4-bfeae647e0a0}', # 3.1a1
    '3.1.102': '{f6e199bf-dc64-42f3-87d4-1525991a013e}', # 3.1a2
    '3.1.111': '{c3c82893-69b2-4676-8554-1b6ee6c191e9}', # 3.1b1
    '3.1.121': '{da2b5170-12f3-4d99-8a1f-54926cca7acd}', # 3.1c1
    '3.1.122': '{bceb5133-e2ee-4109-951f-ac7e941a1692}', # 3.1c2
    '3.1.150': '{3ad61ee5-81d2-4d7e-adef-da1dd37277d1}', # 3.1.0
}
