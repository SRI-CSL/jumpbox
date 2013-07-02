from twisted.web.resource import Resource, NoResource
from pprint import pformat

import time;




class Stegotorus(Resource):
   #delay can be 0; 1 just quietens things down; 0.1 is OK too.
   delay = 1
   stegotorus = 'http://127.0.0.1:8000/stegotorus/ss_push'
   cycle = 0

   def __init__(self):
      Resource.__init__(self)


   def getChild(self, name, request):
      if name == '':
         return self
      elif name == 'jb_pull':
         return Pull()
      elif name == 'jb_push':
         return Push()
      elif name == 'ss_push':
         return Server()
      else:
         return NoResource()



class Pull(Resource):

   def render_GET(self, request):
      print 'GET: Pull %s\n' % Stegotorus.cycle
      #this the the "get next packet" request
      #need to set DJB_URI and DJB_METHOD at least
      request.setHeader('DJB_METHOD', 'POST');
      request.setHeader('DJB_URI', Stegotorus.stegotorus);
      self.make_clientCookie(request);
      time.sleep(Stegotorus.delay)
      return "<html><body>GET: Pull</body></html>"

   
   def make_clientCookie(self, request):
      cookie = 'clientId=req'+str(Stegotorus.cycle)+'; '
      cookie += 'money=US'+str(Stegotorus.cycle)
      request.setHeader('DJB_COOKIE', cookie)


class Push(Resource):
    
   def render_POST(self, request):
      print 'POST: Push %s\n' % Stegotorus.cycle
      print 'End of cycle: %s\n' % Stegotorus.cycle
      Stegotorus.cycle += 1
      time.sleep(Stegotorus.delay)
      return "<html><body>POST: Push</body></html>"
    

class Server(Resource):
      
   def render_GET(self, request):
      print 'GET: Server %s\n' % Stegotorus.cycle
      self.make_serverCookie(request);  
      #print pformat(list(request.requestHeaders.getAllRawHeaders()))
      time.sleep(Stegotorus.delay)
      return "<html><body>GET: Server</body></html>"
    
   def render_POST(self, request):
      print 'POST: Server %s\n' % Stegotorus.cycle
      self.make_serverCookie(request);
      #print pformat(list(request.requestHeaders.getAllRawHeaders()))
      time.sleep(Stegotorus.delay)
      return "<html><body>POST: Server </body></html>"
    
   def make_serverCookie(self, request):
      cookie = 'baz=post'+str(Stegotorus.cycle)+'; '
      cookie += 'golum=crud'+str(Stegotorus.cycle)
      request.setHeader('Set-Cookie', cookie)
