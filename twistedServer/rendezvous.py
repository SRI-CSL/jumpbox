from twisted.web.resource import Resource

import tempfile

class Rendezvous(Resource):
    #fake everything, till the real thing comes along.
    server = 'vm06.csl.sri.com'
    secret = 'U4Sv7k2PY0Gq7TFi'
    freedom_request = 'http://vm06.csl.sri.com/photos/26907150@N08/1457660969/lightbox?_utma=ACbLoAX643zHB8Bqb5MtfZWLUdfMZDUvH9hthuYoM96yRlIIBtPY1ns1kfEh72EFYUOr0sxHuBs2PiQ4WJf9RVNqCUaaDJabwIv8S5g8Ld1zNhoB4lc8QuqjqjVmk98P9qNJbOBpLQLHTU5Jo1f6koLiS1diUEEpTXRVsWzDKHsA&_utmz=Gu8jdzMURgtpNVnP6odSZVGKBEE='
    nep =   "ThisIsANet"
    onion_type = -1
    pow_status = 30

    def __init__(self, name=None):
        Resource.__init__(self)
        self.name = name

    def getChild(self, name, request):
        return Rendezvous(name)

    def render_GET(self, request):
        print 'GET: %s\n' % self.name
        if self.name == 'reset':
            Rendezvous.onion_type = -1
            Rendezvous.pow_status = 30
        return "<html><body>GET: Rendezvous </body></html>"

    def render_POST(self, request):
        if self.name == 'gen_request':
            content = request.content.read()
            print 'POST: %s submitted %s\n' % (self.name, content)
            return Rendezvous.freedom_request 
        elif self.name == 'image':
            #need to pass back the file url of the image.
            imagefile = tempfile.NamedTemporaryFile(mode='wb', suffix='.jpg', prefix='onion', dir=None, delete=False)
            imagefile.write(request.content.read())
            path = imagefile.name
            imagefile.close()
            request.setHeader('Content-Type', 'application/json')
            print 'POST: %s submitted %s which I wrote to %s\n' % (self.name, request.getHeader('Content-Length'), path)
            #report back the image, and that the onion is signed
            Rendezvous.onion_type = 3
            return '{ "image": "file://' + path + '", "onion_type": 3}'
        elif self.name == 'peel':
            content = request.content.read()
           #onion_type:  { BASE : 0, POW : 1, CAPTCHA : 2, SIGNED : 3, COLLECTION : 4 },
            if Rendezvous.onion_type == 0:
                pass
            elif Rendezvous.onion_type == 1:
                pass
            elif Rendezvous.onion_type == 2:
                pass
            elif Rendezvous.onion_type == 3:
                print 'POST: %s submitted %s (pow_status = %s)\n' % (self.name, content, Rendezvous.pow_status)
                if Rendezvous.pow_status <= 100:
                    retval = '{ "type": 1,  "info": %s }' % Rendezvous.pow_status
                    Rendezvous.pow_status += 5
                    return retval
                else:
                    return '{ "type": 0,  "info": "%s" }' % Rendezvous.nep

    
            



                





