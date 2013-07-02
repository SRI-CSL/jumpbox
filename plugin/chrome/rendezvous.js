/*
 *
 *  Given the name of a server:
 *  Step 0: Construct a mod_freedom defiance request (by asking the JumpBox - an XHR).
 *  Step 1: Make the mod_freedom request (an over-the-wire XHR).
 *  Step 2: Display the image & forward the image to the JumpBox (this should be interesting). Might
 *   be better to do this in the reverse order, since the jump box could write it out to a file, and
 *   let us know its file url.
 *

 http://blogs.adobe.com/webplatform/2012/01/17/displaying-xhr-downloaded-images-using-the-file-api/
 http://stackoverflow.com/questions/9977046/getting-an-image-from-an-xmlhttprequest-and-displaying-it
 http://www.philten.com/us-xmlhttprequest-image/
 http://jsperf.com/encoding-xhr-image-data/14


 *  Step 3: Ask for the status of the onion (should respond with a status and perhaps amn activity)
 *  Step 4: Repeat step 3 until we have a net. Once we have a net we need to do the dance.
 *  Ideas on this Jeroen?
 */









var rendezvousGenerator = {

  populate: function () {
 

    }
  
};

document.addEventListener('DOMContentLoaded', function () {
        rendezvousGenerator.populate();
    });

