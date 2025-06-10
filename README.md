# RocketG ame

Rocket game

## Build image

```powershell
$resourceGroup = "rg-rocket"
$acrName = "rocket0000010"
$imageServer = "rocketserver"
$imageConsole = "rocketconsole"
$location = "northeurope"

# Create resource group
az group create --name $resourceGroup --location $location

# Create ACR
az acr create --resource-group $resourceGroup --name $acrName --sku Basic

az acr update -n $acrName --admin-enabled true
$acrPassword=$(az acr credential show -n $acrName --query passwords[0].value -o tsv)

# Build image
cd src/cpp/RocketServer
docker build -t "$acrName.azurecr.io/$imageServer" .
cd ..\RocketConsole
docker build -t "$acrName.azurecr.io/$imageConsole" .

# Test image
docker run -it --rm -p 3501:3501/udp --name $imageServer "$acrName.azurecr.io/$imageServer"
docker run -it --rm --name $imageConsole "$acrName.azurecr.io/$imageConsole"

# Login to ACR
az acr login --name $acrName

# Build images in ACR
az acr build --registry $acrName --image "$imageServer" ./src/cpp/RocketServer
az acr build --registry $acrName --image "$imageConsole" ./src/cpp/RocketConsole

# Push images
docker push "$acrName.azurecr.io/$imageServer"
docker push "$acrName.azurecr.io/$imageConsole"

# Deploy to ACI
az container create --resource-group $resourceGroup --name "$imageServer" --image "$acrName.azurecr.io/$imageServer" --cpu 1 --memory 1 --registry-login-server "$acrName.azurecr.io" --registry-username $acrName --registry-password $acrPassword --ports 3501 --protocol UDP --ip-address public --restart-policy Never --os-type linux
```
