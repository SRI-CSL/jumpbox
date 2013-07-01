var popupGenerator = {

  populate: function () {
        var img = document.createElement('img');
        img.src = 'jumpBox.png';
        img.setAttribute('alt', 'jumpBox');
        document.body.appendChild(img);
    }
  
};

document.addEventListener('DOMContentLoaded', function () {
        popupGenerator.populate();
    });
