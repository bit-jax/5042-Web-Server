function generateDynamicContent() {
    document.getElementById("dyn").innerHTML = new Date().toLocaleString();
}

function quit() {
    document.getElementById('other_content').src = "/quit";
}

function some_shakespeare() {
    document.getElementById('other_content').src = "/shakespeare?start=1068896&length=41";
}

function all_shakespeare() {
    document.getElementById('other_content').src = "/shakespeare.txt";
}

function env() {
    document.getElementById('other_content').src = "/env";
}