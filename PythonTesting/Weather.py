import urllib.request
import json
from datetime import date, timedelta
import pickle

download = False # make this true to download fresh data
forecast = {}

if(download):
	#f = urllib.request.urlopen('http://api.wunderground.com/api/' API_KEY '/astronomy/hourly/q/' CITY '.json')
	f = urllib.request.urlopen('https://gist.githubusercontent.com/pdiefend/fb5f8244f24b61e77e8debe6415fb2d5/raw/fc3cb965c996988fb8cfe7a644bad399b64f7337/testResponse.json')
	json_string = f.read()
	
	jsonFile = open('rqst.json', 'w')
	jsonFile.write(json_string.decode('utf-8'))
	jsonFile.close()
	
	forecast = json.loads(json_string.decode('utf-8'))

	pickle.dump(forecast, open('forecast.p', 'wb'))
else:
	forecast = pickle.load(open('forecast.p', 'rb'))

# Process the data using the full power of python (i.e. the easy way)
# Start with temperatures
temperatures = []
for i in range(0,24):
	temperatures.append(int(forecast['hourly_forecast'][i]['temp']['english'])) 
# zero is the next hour so we'll have to download at 2300 or later each night

# Get the sunrise in minutes, we can convert as needed.
sunrise = int(forecast['sun_phase']['sunrise']['hour'])*60 + int(forecast['sun_phase']['sunrise']['minute'])
sunset = int(forecast['sun_phase']['sunset']['hour'])*60 + int(forecast['sun_phase']['sunset']['minute'])
firstlight = sunrise - 30 # due to the granularity of our setup, we'll cut a corner and just add
lastlight = sunset + 30 # or subtract to get first or last light

chanceOfPrecip = []
for i in range(0,24):
	chanceOfPrecip.append(int(forecast['hourly_forecast'][i]['pop']))
	
print('Temperatures: ' + str(temperatures))
print('Precip Chances: ' + str(chanceOfPrecip))
print('Sunrise: ' + str(sunrise) + ', First Light: ' + str(firstlight))
print('Sunset: ' + str(sunset) + ', Last Light: ' + str(lastlight))

# ======================================================================================

# Okay, now process the data the hard way, the C way we'll have to on the uC
# We can probably read the data line by line looking for temperature, pop, etc
# then parse out the data we want.
# The sunrise and sunset may be a little more difficult but not impossible.

# Can we read the data fast enough / will the buffer be large enough for the request?


#EOF
