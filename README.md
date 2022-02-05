# Currency-Checker
Currency Checker shows the prices and price changes of FIAT currencies along with cryptocurrencies. 

For FIAT currencies this url is used "https://www.x-rates.com/table/?from=USD&amount=1" / The websites provides an update of the prices nearly each 15 minutes.
For Cryptocurrencies the coinmarketcap api with basic plan is used, the update rate is every few seconds, but an account is required to get the APIKEY, then to be submitted along the HTTP request, replace [APIKET] with your APIKEY inside the source line number: 
If one APIKEY is used, you can update every 10 mins, if you want to update more frequently, create more accounts for more APIKEYs
With the basic plan, 333 updates is allowed every day, means nearly 13 updates is allowed every hour with a single APIKEY, 60 / 13 = 4.6 minutes b/w each update. 




