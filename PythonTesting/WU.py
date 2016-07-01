import urllib.request
import json
from datetime import date, timedelta
import passwords

class WU:
    #data = {}
    key = passwords.WU_KEY
    #city = 'Lewisburg'

    def __init__(self, city):
        self.city = city
        #self.refresh()

    def refresh(self):
        f = urllib.request.urlopen('http://api.wunderground.com/api/'+self.key+'/conditions/hourly/astronomy/forecast/q/PA/'+self.city+'.json')
        json_string = f.read()
        self.data = json.loads(json_string.decode("utf-8"))
        return

    def forecast_high(self, day):
        return self.data['forecast']['simpleforecast']['forecastday'][day]['high']['fahrenheit']

    def forecast_low(self, day):
        return self.data['forecast']['simpleforecast']['forecastday'][day]['low']['fahrenheit']

    def current_temp(self):
        return self.data['current_observation']['temp_f']

    def current_condition(self):
        return self.data['current_observation']['weather']

    def hourly_forecast(self):
        forecast=[]
        sunrise = int(self.data['sun_phase']['sunrise']['hour'])
        sunset =  int(self.data['sun_phase']['sunset']['hour'])
        
        for hour in self.data['hourly_forecast']:
            if(int(hour['FCTTIME']['mday']) == (date.today()+timedelta(1)).day):
                d = {}
                d['hour'] = hour['FCTTIME']['hour']
                d['temp'] = hour['temp']['english']
                d['condition'] = hour['condition']
                if((int(d['hour']) > sunrise) and (int(d['hour']) < sunset)):
                    d['daylight'] = True
                else:
                    d['daylight'] = False
                forecast.append(d)
        return forecast

# Usage:
# from WU import WU
# w = WU('Lewisburg')
# w.refresh() to get up-to-date data
# print(w.forecast_high(0))

