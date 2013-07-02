from twisted.web.resource import Resource



class Rendezvous(Resource):

   def __init__(self, name=None):
      Resource.__init__(self)
      self.name = name

   def getChild(self, name, request):
      return Rendezvous(name)

   def render_GET(self, request):
      print 'GET: %s\n' % self.name
      return "<html><body>GET: Rendezvous </body></html>"
