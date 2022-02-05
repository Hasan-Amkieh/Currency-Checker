// DISCLIAMER: 
/*
1- THE DISPLAY DOES NOT WORK WHEN IT IS COMPILED ON VISUAL STUDIO CODE FOR SOME REASON, BUT IN ARDUINO IDE IT DOES
2- The EEPROM did not work at all with the current program, so I decided to store the data in the DRAM instead
3- In the Arduino IDe, the default corfe is 1, use core 0 for the animation stuff

*/

//#define TEST // Unleash some test prices for the FIAT currencies

#include <Arduino.h>
#include <string>

#include <TFT_eSPI.h>
#include <SPI.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>

#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "PASSWORD"

#define DEFAULT_DISPLAY_COLOR TFT_BLACK
#define DEFAULT_FONT 1
#define VERTICAL_PADDING 4
#define LAST_UPDATE_HEIGHT 28
#define INCREASING_COLOR 0x05E0 // 0x03E0 for DARK GREEN / 0x07E0 for DARK GREEN
#define DECREASING_COLOR TFT_RED
TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h

WiFiMulti wifiMulti;

String http_request_FIAT = "https://www.x-rates.com/table/?from=USD&amount=1";
String http_request_Cryptos = "https://pro-api.coinmarketcap.com/v1/cryptocurrency/quotes/latest?symbol=BTC,ETH,ADA,XRP,SHIB,LTC,SOL,AVAX,DOGE";

enum Currencies_ID {
  TRY, EUR, GBP, AUD, BTC, ETH, ADA, XRP, SHIB, LTC, SOL, AVAX, DOGE
};

#define NUMBER_OF_CURRENCIES 13
#define NUMBER_OF_FIATS 4

#define CRYPTO_API_KEY_1 "b8fa1810-9abb-4f5c-b0bd-2863a01aff4f"

String currency_names[NUMBER_OF_CURRENCIES] = {"Turkish Lira", "Euro", "British Pound", "Australian Dollar", "Bitcoin", "Ethereum", "Cardano", "Ripple", "Shiba-Inu", "Litecoin", "Solana", "Avalanche", "Dogecoin"};
String currency_ISO[NUMBER_OF_CURRENCIES] = {"TRY", "EUR", "GBP", "AUD", 
                                              "BTC", "ETH", "ADA", "XRP", "SHIB", "LTC", "SOL", "AVAX", "DOGE"
};

String currency_indicators[NUMBER_OF_CURRENCIES] = {"https://www.x-rates.com/graph/?from=USD&amp;to=TRY", 
                                                    "https://www.x-rates.com/graph/?from=USD&amp;to=EUR", 
                                                    "https://www.x-rates.com/graph/?from=USD&amp;to=GBP",
                                                    "https://www.x-rates.com/graph/?from=USD&amp;to=AUD"};

class Pair {
  public:
  Currencies_ID id1, id2;
  String pair_name;
  Pair () {}
  Pair (Currencies_ID id1_, Currencies_ID id2_) {
    id1 = id1_;
    id2 = id2_;
    pair_name = String();
    pair_name.concat(currency_ISO[id1]);
    pair_name.concat('/');
    pair_name.concat(currency_ISO[id2]);
  }

  float calculate_price(); // is actually defined below

};

class Currency {
public:
  Currencies_ID id;
  float price;
  float last_24hr_change;

  // The list of pairs to be listed on the display:
  Pair *pairs;
  uint8_t number_of_pairs;

  Currency() : number_of_pairs(0){}

  public:
  Currency(Currencies_ID cur, uint8_t number_of_pairs_) {
    id = cur;
    number_of_pairs = number_of_pairs_;
    if (number_of_pairs == 0) return;
    pairs = new Pair[number_of_pairs];
    Pair TRY_pairs[] = {Pair(EUR, TRY), Pair(GBP, TRY), Pair(AUD, TRY)};;
    Pair EUR_pairs[] = {Pair(GBP, EUR), Pair(AUD, EUR)};
    Pair GBP_pairs[] = {Pair(EUR, GBP), Pair(AUD, GBP)};
    Pair AUD_pairs[] = {Pair(EUR, AUD), Pair(GBP, AUD)};
    
    for (int i = 0 ; i < number_of_pairs ; i++) {
      switch(cur) {
        case TRY:
          pairs[i] = TRY_pairs[i];
          break;
        case EUR:
          pairs[i] = EUR_pairs[i];
          break;
        case GBP:
          pairs[i] = GBP_pairs[i];
          break;
        case AUD:
          pairs[i] = AUD_pairs[i];
          break;
      }
    }
  }
  
};

class CryptoInfo {

  public:
  double last_1h_change = 0;
  double last_24h_change = 0;
  double last_7d_change = 0;

};

#define DEFAULT_TIME_TO_WAIT 5 /*mins*/ * 60 /*mins to secs*/ * 1000 /*secs to millis*/
unsigned long to_wait = 0;
// Check every 1 hour 5.5 times, 60/5.5 = 10 mins, means every 10 mins check one time
unsigned long last_check = 0;

String time_stamp_to_find = "ratesTableTimestamp";
String time_stamp;
String day_stamp, month_stamp, year_stamp, hour_stamp, minutes_stamp;

Currency currencies[NUMBER_OF_CURRENCIES] =
 {Currency(TRY, 3)
 ,Currency(EUR, 2)
 ,Currency(GBP, 2)
 ,Currency(AUD, 2)
 ,Currency(BTC, 0)
 ,Currency(ETH, 0)
 ,Currency(ADA, 0)
 ,Currency(XRP, 0)
 ,Currency(SHIB, 0)
 ,Currency(LTC, 0)
 ,Currency(SOL, 0)
 ,Currency(AVAX, 0)
 ,Currency(DOGE, 0)
};

CryptoInfo crypto_info[NUMBER_OF_CURRENCIES - NUMBER_OF_FIATS];

String months[] = {"Jan", "Feb" ,"Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
inline String convert_month_to_num(String month) { // inline is better cause it is called only once!
  int i;
  for (i = 0 ; i < 12 ; i++) {
    if (months[i].equals(month)) break;
  }
  
  return String(i+1);
}

float Pair::calculate_price() {

  return currencies[id2].price / currencies[id1].price;

}

bool isLastUpdateShown = false;

void drawLastUpdate( ) {

  String msg = "Last Update : ";
  tft.setTextColor(TFT_BLUE);
  tft.fillRect(0, 101, 128, 27, DEFAULT_DISPLAY_COLOR);
  tft.drawString(msg, 10, 105);
  tft.drawString(time_stamp, 10, 115);

  isLastUpdateShown = true;

}

#define NUMBER_OF_ENTITIES_FOR_CURRENCY 24 /*For the 24h change*/ 

class Entity {
  public:
  uint8_t mins;
  uint8_t hrs;
  uint8_t days;
  uint8_t months;
  uint16_t yrs; // 6 bytes

  float price = 0; // 4 bytes

  Entity() {}

  Entity(uint8_t mins_, uint8_t hrs_, uint8_t days_, uint8_t months_, uint16_t yrs_, float price_) {
    mins = mins_;
    hrs = hrs_;
    days = days_;
    months = months_;
    yrs = yrs_;

    price = price_;
  }

};

int convert_mins_to_index(int mins) {

  int entity_index;

  if (mins >= 0 && mins <= 9)
    entity_index = 0;
  else if (mins >= 10 && mins <= 19)
    entity_index = 1;
  else if (mins >= 20 && mins <= 29)
    entity_index = 2;
  else if (mins >= 30 && mins <= 39)
    entity_index = 3;
  else if (mins >= 40 && mins <= 49)
    entity_index = 4;
  else if (mins >= 50 && mins <= 59)
    entity_index = 5;

  return entity_index; 

}

Entity currency_entities[NUMBER_OF_FIATS][NUMBER_OF_ENTITIES_FOR_CURRENCY];
// And the same applies for the last 24hrs change (0 - 23)
// Is only needed for FIATS, Coinmarketcap provides the last 24h/1h price changes

uint8_t animation_program = 0; // 0 for none, 1 for the ".Connecting." , 2 for ".Updating."


uint8_t animattionStatus = 0; // 0 - stopped, 1 - to start, 2 - to stop
// The second core is responsible for the animation, when the state is 2, 
// the second core is responsible for stopping the animation and setting the state to 0, 

void setup() {

  Serial.begin(115200);

  #ifdef TEST 

  Entity e;
  e.price = 0.5;
  currency_entities[TRY][21] = e;

  e = Entity();
  e.price = 0.35;
  currency_entities[EUR][21] = e;

  e = Entity();
  e.price = 0.5;
  currency_entities[GBP][21] = e;

  e = Entity();
  e.price = 3.5;
  currency_entities[AUD][21] = e;


  /*e = Entity();
  e.price = 1.9;
  currency_entities[TRY][21] = e;

  e = Entity();
  e.price = 1.0;
  currency_entities[EUR][21] = e;

  e = Entity();
  e.price = 0.9;
  currency_entities[GBP][21] = e;

  e = Entity();
  e.price = 0.655;
  currency_entities[AUD][21] = e;*/

  #endif

  tft.init();
  tft.setRotation(2);
  tft.setTextColor(TFT_BLUE);
  tft.fillScreen(DEFAULT_DISPLAY_COLOR);
  tft.drawCentreString("Currency Checker", 64, 64, 2);
  tft.drawCentreString("By Hasan Amkieh", 64, 80, 2);
  
  delay(4000);

  tft.fillScreen(DEFAULT_DISPLAY_COLOR);
  tft.drawCentreString(".Connecting.", 64, 64, DEFAULT_FONT);

  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  { // The "connecting..." dots animation:
    String original_msg = "Connecting";
    String msg;
    String dots;
    for (int i = 1 ; wifiMulti.run() != WL_CONNECTED ; i++) {
      msg = String();
      dots = String();
      if (i == 4) {
        i = 1;
      }
      for (int index = i ; index > 0 ; index--) {
        dots.concat('.');
      }
      msg.concat(dots);
      msg.concat(original_msg);
      msg.concat(dots);
      tft.fillScreen(DEFAULT_DISPLAY_COLOR);
      tft.drawCentreString(msg, 64, 64, DEFAULT_FONT);
      delay(100);
    }
  }

}

void loop() {


  if (millis() - last_check >= to_wait) {
    String http_request;
    for (uint8_t turn = 0 ; turn < 2 ; turn++) { // 0 for FIAT / 1 for Cryptos
      http_request = turn == 0 ? http_request_FIAT : http_request_Cryptos;
      to_wait = DEFAULT_TIME_TO_WAIT; // This is only to bypass the first time check
      if((wifiMulti.run() == WL_CONNECTED)) {

        tft.fillScreen(DEFAULT_DISPLAY_COLOR);
        tft.setTextColor(TFT_BLUE);
        tft.drawCentreString(".Updating.", 64, 64, DEFAULT_FONT);
        
        Serial.println("Beginning HTTP request");

        HTTPClient http;
        http.begin(http_request);
        if (turn == 1) { // if for Cryptos
          http.addHeader("Accepts", "application/json");
          http.addHeader("X-CMC_PRO_API_KEY", CRYPTO_API_KEY_1); // Currently there is only one apikey
        }
        Serial.printf("[GET] : %s\n", http_request.c_str());

        // start connection and send HTTP header
        signed int httpCode = http.GET();
        Serial.printf("HTTP return code: %d\n", httpCode);

        // httpCode will be negative on error
        if(httpCode > (signed int)0) {

          // file found at server
          if(httpCode == HTTP_CODE_OK) {
            last_check = millis();
            String payload = http.getString();
            Serial.printf("The payload size is : %ld\n", payload.length());

            if (turn == 0) { // FIATs
              { // Time stamp
                unsigned int index = payload.indexOf(time_stamp_to_find, 460)+19+2; 
                time_stamp = payload.substring(index, payload.indexOf('<', index));
                Serial.printf("Time Stamp: %s\n", time_stamp.c_str());

                month_stamp = convert_month_to_num(time_stamp.substring(0, time_stamp.indexOf(' ')));
                day_stamp = time_stamp.substring(time_stamp.indexOf(' ')+1, time_stamp.indexOf(','));
                year_stamp = time_stamp.substring(time_stamp.indexOf(',')+2, time_stamp.indexOf(',')+2+5);
                hour_stamp = time_stamp.substring(time_stamp.indexOf(':')-2, time_stamp.indexOf(':'));
                minutes_stamp = time_stamp.substring(time_stamp.indexOf(':')+1, time_stamp.indexOf(':')+3);

                int hour = hour_stamp.toInt();
                if (hour < 21) {
                  hour_stamp = String(hour + 3);
                } else {
                  hour_stamp = "0";
                  hour_stamp.concat(hour - 21);
                }

                time_stamp = day_stamp;
                time_stamp.concat('-');
                time_stamp.concat(month_stamp);
                time_stamp.concat('-');
                time_stamp.concat(year_stamp);
                time_stamp.concat(' ');
                time_stamp.concat(hour_stamp);
                time_stamp.concat(':');
                time_stamp.concat(minutes_stamp);

                Serial.printf("Time Stamp: %s\n", time_stamp.c_str());
              }

              {
                int value_index;
                Entity entity;
                int mins = minutes_stamp.toInt();
                int hrs = hour_stamp.toInt();
                for (int i = 0 ; i < NUMBER_OF_FIATS ; i++) {
                  value_index = payload.indexOf(currency_indicators[i], 120) + 52;
                  currencies[i].price = payload.substring(value_index, payload.indexOf('<', value_index)).toFloat();

                  entity.mins = mins;
                  entity.hrs = hrs;
                  entity.days = day_stamp.toInt();
                  entity.months = month_stamp.toInt();
                  entity.yrs = year_stamp.toInt();
                  entity.price = currencies[i].price;

                  // 24hrs system
                  if (60 - mins < 10) { // then store it in the 24hrs system
                    int entity_index = hrs;
                    currency_entities[i][entity_index] = entity;
                    Serial.printf("Storing a new entity at index %d", entity_index);
                  }
                  // 24hrs system / END

                  Serial.printf("%s at price %f\n", currency_names[i], currencies[i].price);

                }
              }
            }

            else { // Cryptos: 
              
              uint16_t curr_index, quote_index, info_index;
              String price;
              for (uint8_t i = NUMBER_OF_FIATS ; i < NUMBER_OF_CURRENCIES ; i++) {
                Serial.printf("Getting info for %s\n", currency_ISO[i].c_str());
                curr_index = payload.indexOf(currency_ISO[i].c_str());
                quote_index = payload.indexOf("quote", curr_index);
                info_index = payload.indexOf("price", payload.indexOf("quote", curr_index));
                price = payload.substring(info_index + 7, payload.indexOf(',', info_index));
                currencies[i].price = price.toFloat();
                info_index = payload.indexOf("percent_change_1h", info_index);
                crypto_info[i - NUMBER_OF_FIATS].last_1h_change = payload.substring(info_index + 19, payload.indexOf(',', info_index)).toDouble();
                info_index = payload.indexOf("percent_change_24h", info_index);
                crypto_info[i - NUMBER_OF_FIATS].last_24h_change = payload.substring(info_index + 20, payload.indexOf(',', info_index)).toDouble();
                info_index = payload.indexOf("percent_change_7d", info_index);
                crypto_info[i - NUMBER_OF_FIATS].last_7d_change = payload.substring(info_index + 19, payload.indexOf(',', info_index)).toDouble();
                
                Serial.printf("last 1h change: %.2f\n  ---\n", crypto_info[i - NUMBER_OF_FIATS].last_1h_change);

              }

            }

          }
          else {
            Serial.println("Server error!");

            ;

          }
        } else if (httpCode == -1) { // No internet connection!
            Serial.println("No internet connection...");
            tft.fillScreen(DEFAULT_DISPLAY_COLOR);
            tft.drawCentreString("No internet connection...", 64, 64, DEFAULT_FONT);
        }
        else { // problem is not associated with the server: 
          Serial.println("Unknown network error, please check the network");
          Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
          tft.fillScreen(DEFAULT_DISPLAY_COLOR);
          tft.drawCentreString("Check your network", 64, 64, DEFAULT_FONT);
        }

        http.end();

        printf("Waiting for %ld till the next update!\n", to_wait / 1000);
      }
      else {
        Serial.printf("Trying to connect to \"%s\"!\n", WIFI_SSID);
        delay(1000);
      }
    }
  } // End of the for-loop update block

  float remaining_height;
  int sentence_w, excess_space, h; // all the variables needed for the price change arrows
  bool isIncreasing;
  int pairMargin;
  String str;
  for (uint8_t turn = 0 ; turn < 2 ; turn++) {
    if (turn == 0) { // For FIATs
      // Update Currencies page:
      Currency currency;
      float price_change;
      float oldPrice, currentPrice;
      for (int i = 0 ; i < NUMBER_OF_FIATS ; i++) {
        currency = currencies[i];
        if ( remaining_height / (currency.number_of_pairs + 1) >= 30.0 ) {
          drawLastUpdate(); // isLastUpdateShown will be set to true
          remaining_height = (tft.height() - VERTICAL_PADDING * 2 - LAST_UPDATE_HEIGHT);
        } else {
          isLastUpdateShown = false;
          remaining_height = tft.height() - VERTICAL_PADDING * 2;
          if (remaining_height / (currency.number_of_pairs + 1) < 30) 
            Serial.println("[ERROR] There is not enough space for currency pairs!!!");
        }
        pairMargin = remaining_height / (currency.number_of_pairs + 1);

        String str = String();
        if (isLastUpdateShown)
          tft.fillRect(0, 0, 128, 128 - LAST_UPDATE_HEIGHT + 1, TFT_WHITE);
        else 
          tft.fillRect(0, 0, 128, 128, TFT_WHITE);

        // 24hrs change: 
        str = String();
        currentPrice = currency.price;
        int index_pos = hour_stamp.toInt();
        oldPrice = currency_entities[i][index_pos == 23 ? 0 : index_pos + 1].price;
        Serial.printf("Old price of %s is %f at hour %d\n", currency_names[i].c_str(), oldPrice, index_pos == 23 ? 0 : index_pos + 1);
        if (oldPrice != 0) {
          price_change = ((currentPrice - oldPrice) / oldPrice) * 100;
          if (price_change < 0) {
            tft.setTextColor(DECREASING_COLOR);
            isIncreasing = false;
            price_change = -1 * price_change; // always keep it positive
          }
          else {
            tft.setTextColor(INCREASING_COLOR);
            isIncreasing = true;
          }
          str.concat("24h   %");str.concat(String(price_change, 2));
        } else {
          str.concat("None");
          tft.setTextColor(INCREASING_COLOR);
        }
        h = VERTICAL_PADDING + (int)(0.5*((int)pairMargin)) + 5;
        sentence_w = tft.drawCentreString(str, 64, h, 1);
        if (sentence_w > 36) { // then there is a current change price:
          Serial.println("Drawing Triangle");
          excess_space = (sentence_w - 36) /*percent width*/ - (sentence_w / 2); // this is the excess width on the left side from the percent
          if (isIncreasing) {
            tft.fillTriangle(64 - 12 - excess_space, h + 7, 64 - 4 - excess_space, h + 7, 64 - 8 - excess_space, h, INCREASING_COLOR);
            Serial.printf("The price is increasing by %.2f\n\n", price_change);}
          else {
            tft.fillTriangle(64 - 12 - excess_space, h, 64 - 4 - excess_space, h, 64 - 8 - excess_space, h + 7, DECREASING_COLOR);
            Serial.printf("The price is decreasing by %.2f\n\n", price_change);}
        }

        // 24hrs change // END

        str = String();
        str.concat(String(currency.price, 4));str.concat(' ');
        str.concat(currency_ISO[i]);
        tft.drawCentreString(str, 64, VERTICAL_PADDING, 2);

        Pair pair;
        for (int index = 0 ; index < currency.number_of_pairs ; index++) {
          pair = currency.pairs[index];
          currentPrice = pair.calculate_price();

          str = String();
          int ind = index_pos == 23 ? 0 : index_pos + 1;
          if (currency_entities[pair.id1][ind].price != 0)
            oldPrice = currency_entities[pair.id2][ind].price / currency_entities[pair.id1][ind].price;
          else oldPrice = 0;

          if (oldPrice != 0) {
            price_change = ((currentPrice - oldPrice) / oldPrice) * 100;
            if (price_change < 0) {
              tft.setTextColor(DECREASING_COLOR);
              isIncreasing = false;
              price_change = -1 * price_change; // always keep it positive 
            }
            else {
              tft.setTextColor(INCREASING_COLOR);
              isIncreasing = true;
            }
            str.concat("24h   %");str.concat(String(price_change, 2));
          } else {
            str.concat("None");
            tft.setTextColor(INCREASING_COLOR);
          }
          h = VERTICAL_PADDING + (index + 1)*((int)pairMargin) + (int)(0.5*((int)pairMargin)) + 5;
          sentence_w = tft.drawCentreString(str, 64, h, 1);
          if (sentence_w > 36) { // then there is a current change price:
            Serial.println("Drawing Triangle");
            excess_space = (sentence_w - 36) /*percent width*/ - (sentence_w / 2); // this is the excess width on the left side from the percent
            if (isIncreasing) {
              tft.fillTriangle(64 - 12 - excess_space, h + 7, 64 - 4 - excess_space, h + 7, 64 - 8 - excess_space, h, INCREASING_COLOR);
              Serial.printf("The price is increasing by %.2f\n\n", price_change);}
            else {
              tft.fillTriangle(64 - 12 - excess_space, h, 64 - 4 - excess_space, h, 64 - 8 - excess_space, h + 7, DECREASING_COLOR);
              Serial.printf("The price is decreasing by %.2f\n\n", price_change);}
          }

          str = String();
          str.concat(String(currentPrice));
          str.concat(' ');
          str.concat(pair.pair_name);
          tft.drawCentreString(str, 64, VERTICAL_PADDING + (index + 1)*((int)pairMargin), 2);

        }

        delay(5000);
      }
    }
    else { // For Cryptos: 
      str = String();
      remaining_height = (tft.height() - VERTICAL_PADDING * 2);
      pairMargin = remaining_height / (3 + 1); // 3 for last 1h/24h/7d changes, 1 for price
      float last_change;
      for (uint8_t i_ = NUMBER_OF_FIATS ; i_ < NUMBER_OF_CURRENCIES ; i_++) {

        if (i_ >= NUMBER_OF_CURRENCIES) Serial.println("The loop should stop!");
        Serial.printf("Currency: %s index: %d\n", currency_ISO[i_].c_str(), i_);

        tft.fillScreen(DEFAULT_DISPLAY_COLOR);

        isLastUpdateShown = false;

        if (isLastUpdateShown)
          tft.fillRect(0, 0, 128, 128 - LAST_UPDATE_HEIGHT + 1, TFT_WHITE);
        else 
          tft.fillRect(0, 0, 128, 128, TFT_WHITE);

        // 7d change: // 72 pixels off
        str = String();
        last_change = crypto_info[i_ - NUMBER_OF_FIATS].last_7d_change;
        last_change = crypto_info[i_ - NUMBER_OF_FIATS].last_7d_change > 0.0 ? last_change : last_change * -1;
        isIncreasing = crypto_info[i_ - NUMBER_OF_FIATS].last_7d_change > 0.0;
        if (isIncreasing) tft.setTextColor(INCREASING_COLOR);
        else tft.setTextColor(DECREASING_COLOR);
        h = VERTICAL_PADDING + (1)*((int)pairMargin) + 5;
        str.concat("7d change   %");
        str.concat(String(last_change, 2).toDouble());
        sentence_w = tft.drawCentreString(str, 64, h, 1);

        excess_space = (sentence_w - 72) /*percent width*/ - (sentence_w / 2);
        if (isIncreasing) {
          tft.fillTriangle(64 - 12 - excess_space, h + 7, 64 - 4 - excess_space, h + 7, 64 - 8 - excess_space, h, INCREASING_COLOR);
        }
        else {
          tft.fillTriangle(64 - 12 - excess_space, h, 64 - 4 - excess_space, h, 64 - 8 - excess_space, h + 7, DECREASING_COLOR);
        }

        // 24h change: // 84 pixels off
        str = String();
        last_change = crypto_info[i_ - NUMBER_OF_FIATS].last_24h_change;
        last_change = crypto_info[i_ - NUMBER_OF_FIATS].last_24h_change > 0.0 ? last_change : last_change * -1;
        isIncreasing = crypto_info[i_ - NUMBER_OF_FIATS].last_24h_change > 0.0;
        if (isIncreasing) tft.setTextColor(INCREASING_COLOR);
        else tft.setTextColor(DECREASING_COLOR);
        h = VERTICAL_PADDING + (2)*((int)pairMargin) + 5;
        str.concat("24hr change   %");
        str.concat(String(last_change, 2).toDouble());
        sentence_w = tft.drawCentreString(str, 64, h, 1);

        excess_space = (sentence_w - 84) /*percent width*/ - (sentence_w / 2);
        if (isIncreasing) {
          tft.fillTriangle(64 - 12 - excess_space, h + 7, 64 - 4 - excess_space, h + 7, 64 - 8 - excess_space, h, INCREASING_COLOR);
        }
        else {
          tft.fillTriangle(64 - 12 - excess_space, h, 64 - 4 - excess_space, h, 64 - 8 - excess_space, h + 7, DECREASING_COLOR);
        }

        // 1h change:
        str = String();
        last_change = crypto_info[i_ - NUMBER_OF_FIATS].last_1h_change;
        last_change = crypto_info[i_ - NUMBER_OF_FIATS].last_1h_change > 0.0 ? last_change : last_change * -1;
        isIncreasing = crypto_info[i_ - NUMBER_OF_FIATS].last_1h_change > 0.0;
        if (isIncreasing) tft.setTextColor(INCREASING_COLOR);
        else tft.setTextColor(DECREASING_COLOR);
        h = VERTICAL_PADDING + (3)*((int)pairMargin) + 5;
        str.concat("1hr change   %");
        str.concat(String(last_change, 2).toDouble());
        sentence_w = tft.drawCentreString(str, 64, h, 1);

        excess_space = (sentence_w - 78) /*percent width*/ - (sentence_w / 2);
        if (isIncreasing) {
          tft.fillTriangle(64 - 12 - excess_space, h + 7, 64 - 4 - excess_space, h + 7, 64 - 8 - excess_space, h, INCREASING_COLOR);
        }
        else {
          tft.fillTriangle(64 - 12 - excess_space, h, 64 - 4 - excess_space, h, 64 - 8 - excess_space, h + 7, DECREASING_COLOR);
        }

        // Price: 
        str = String();
        str.concat(String(currencies[i_].price, 4));
        str.concat(' ');
        str.concat(currency_ISO[i_]);
        
        tft.setTextColor(INCREASING_COLOR);
        tft.drawCentreString(str, 64, VERTICAL_PADDING, 2);

        delay(4000);
      }
    }
  }

}
