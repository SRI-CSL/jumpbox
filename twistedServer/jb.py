#!/usr/bin/env python

from twisted.internet import reactor
from twisted.web.resource import Resource
from twisted.web.server import Site

from pprint import pformat

import time;

class StegoClient(Resource):
   def getChild(self, name, request):
       if name == '':
           return self
       else:
           return StegoResponse(name)

   def render_GET(self, request):
       return "<html><body>I'm the Jumpbox/Stegotorus standin, till the real thing comes along</body></html>"



class StegoResponse(Resource):
   #delay can be 0; 1 just quietens things down; 0.1 is OK too.
   delay = 1
   stegotorus = 'http://127.0.0.1:8000/ss_push'
   cycle = 0

   def __init__(self, name):
      Resource.__init__(self)
      self.name = name
      
   def render_GET(self, request):
      print 'GET: %s %s\n' % (self.name, StegoResponse.cycle)
      
      if self.name.startswith('jb_pull'):
         #this the the "get next packet" request
         #need to set DJB_URI and DJB_METHOD at least
         request.setHeader('DJB_METHOD', 'POST');
         request.setHeader('DJB_URI', StegoResponse.stegotorus);
         self.make_clientCookie(request);
      elif self.name.startswith('ss_push'):
         self.make_serverCookie(request);  
      #print pformat(list(request.requestHeaders.getAllRawHeaders()))
      time.sleep(StegoResponse.delay)
      return "<html><body>GET: %s</body></html>" % self.name
    
   def render_POST(self, request):
      print 'POST: %s %s\n' % (self.name, StegoResponse.cycle)
      if self.name.startswith('jb_push'):
         print 'End of cycle: %s\n' % StegoResponse.cycle
         StegoResponse.cycle += 1
      elif self.name.startswith('ss_push'):
         self.make_serverCookie(request);
      #print pformat(list(request.requestHeaders.getAllRawHeaders()))
      time.sleep(StegoResponse.delay)
      return "<html><body>POST: %s</body></html>" % self.name
    
   def make_serverCookie(self, request):
      cookie = 'baz=post'+str(StegoResponse.cycle)+'; '
      cookie += 'golum=crud'+str(StegoResponse.cycle)
      request.setHeader('Set-Cookie', cookie)

   def make_clientCookie(self, request):
      cookie = 'clientId=req'+str(StegoResponse.cycle)+'; '
      cookie += 'money=US'+str(StegoResponse.cycle)
      request.setHeader('DJB_COOKIE', cookie)
      


root = StegoClient()
factory = Site(root)

reactor.listenTCP(8000, factory)
reactor.run()



