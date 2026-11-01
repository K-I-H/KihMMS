# MMS settings per operator

The operator can be found automatically from the SIM card's IMSI (`AT+CIMI`: the first five digits are country + network
code). Budget operators usually use the host network's code and settings.

| Country | Operator | MCC-MNC | APN | MMSC | Proxy | Tested |
|---|---|---|---|---|---|---|
| 🇳🇴 Norway | Ice | 242 14 (242 06) | mms.ice.net | http://mms | 10.10.10.10:80 | ✅ send, fetch, OTAMMS |
| 🇳🇴 Norway | Telenor | 242 01 (242 12) | telenor.mms | http://mmsc | mms-proxy.telenor.no:8080 | – |
| 🇳🇴 Norway | Telia | 242 02 (242 08) | mms | http://mmss/ | 193.209.134.132:8080 | – |
| 🇸🇪 Sweden | Telia | 240 01 | mms.telia.se | http://mmss | 193.209.134.132:8080 | – |
| 🇸🇪 Sweden | Tele2 | 240 07 | mms.tele2.se | http://mmsc.tele2.se | 130.244.202.30:8080 | – |
| 🇸🇪 Sweden | Telenor | 240 08 | services.telenor.se | http://mms | 172.30.253.241:8080 | – |
| 🇸🇪 Sweden | Tre | 240 02 | data.tre.se | http://mms.tre.se | 193.209.134.132:8080 | – |
| 🇩🇰 Denmark | TDC | 238 01 | mms | http://mms.tdc.dk:8002 | 194.182.251.15:8080 | – |
| 🇩🇰 Denmark | Telenor | 238 02 / 238 20 | telenor / www.mms.mobil.telenor.dk | http://mms.telenor.dk | 212.88.64.199:80 / 212.88.64.8:8080 | – |
| 🇩🇰 Denmark | Telia | 238 30 | www.mms.mobil.telia.dk | http://mms.telia.dk | 193.209.134.131:8080 | – |

Untested values come from the operators' published phone settings and may be out of date. Pull requests with tested
settings for more operators are very welcome.
