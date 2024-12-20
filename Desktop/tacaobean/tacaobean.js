import axios from "axios";
import { exec } from "node:child_process";

const fetch_url = "https:your/firebase/url_post.json";

let previous_message = "";

async function fetchData(){
    const response = await axios.get(fetch_url);
    if(response.statusText == "OK"){
        // console.log(response.data);
        return response.data;
    }
    else {
        console.log("error while fetching");
        return false
    }
}

async function checkForUpdates(){
    const data = await fetchData();
    if(data.node){
        if(data.node != previous_message){
            console.log("new data available");
            exec(`zenity --notification --text="${data.node}" --icon=icon/path.jpg`);
            previous_message = data.node;
        }
    }
}

async function main(){
    setInterval(async () => {
        console.log("checking for updates");
        await checkForUpdates()
    }, 10000);
}

main();