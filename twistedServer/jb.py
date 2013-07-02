#!/usr/bin/env python

from twisted.internet import reactor
from twisted.web.resource import Resource, NoResource
from twisted.web.server import Site

from stegotorus import Stegotorus
from rendezvous import Rendezvous




class JumpBox(Resource):
   def getChild(self, name, request):
       if name == '':
           return self
       elif name == 'stegotorus':
          return Stegotorus()
       elif name == 'rendezvous':
          return Rendezvous()
       else:
           return NoResource()

   def render_GET(self, request):
       return "<html><body>I'm the Jumpbox/Stegotorus standin, till the real thing comes along</body></html>"








root = JumpBox()
factory = Site(root)

reactor.listenTCP(8000, factory)
reactor.run()



