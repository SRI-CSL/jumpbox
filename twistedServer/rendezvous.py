from twisted.web.resource import Resource


class Rendezvous(Resource):
    #fake everything, till the real thing comes along.
    server = 'vm06.csl.sri.com'
    secret = 'U4Sv7k2PY0Gq7TFi'
    freedom_request = 'http://vm06.csl.sri.com/photos/26907150@N08/1457660969/lightbox?_utma=ACbLoAX643zHB8Bqb5MtfZWLUdfMZDUvH9hthuYoM96yRlIIBtPY1ns1kfEh72EFYUOr0sxHuBs2PiQ4WJf9RVNqCUaaDJabwIv8S5g8Ld1zNhoB4lc8QuqjqjVmk98P9qNJbOBpLQLHTU5Jo1f6koLiS1diUEEpTXRVsWzDKHsA&_utmz=Gu8jdzMURgtpNVnP6odSZVGKBEE='


    def __init__(self, name=None):
        Resource.__init__(self)
        self.name = name

    def getChild(self, name, request):
        return Rendezvous(name)

    def render_GET(self, request):
        print 'GET: %s\n' % self.name
        return "<html><body>GET: Rendezvous </body></html>"

    def render_POST(self, request):
        if self.name == 'gen_request':
            content = request.content.read()
            print 'POST: %s submitted %s\n' % (self.name, content)
            return Rendezvous.freedom_request 
        elif self.name == 'image':
            imagefile = open("onion.jpg", "wb")
            imagefile.write(request.content.read())
            imagefile.close()
            print 'POST: %s submitted %s\n' % (self.name, request.getHeader('Content-Length'))
            return ''

