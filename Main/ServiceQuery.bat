:: In Windows xp, 'sc query' will always exit with status code '0',
:: no matter the query faild or not.

sc query PcapDNSProxyService | find "SERVICE_NAME: PcapDNSProxyService"
