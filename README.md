# RocketG ame

Rocket game

## Build image

```powershell
$resourceGroup = "rg-rocket"
$acrName = "rocket0000010"
$imageServer = "rocketserver"
$imageConsole = "rocketconsole"
$location = "northeurope"

podman machine start

# Create resource group
az group create --name $resourceGroup --location $location

# Create ACR
az acr create --resource-group $resourceGroup --name $acrName --sku Basic

az acr update -n $acrName --admin-enabled true
$acrPassword=$(az acr credential show -n $acrName --query passwords[0].value -o tsv)

# Build image
pushd src/cpp/RocketServer
podman build -t "$acrName.azurecr.io/$imageServer" .
popd

podman build -t "$acrName.azurecr.io/$imageConsole" -f src/cpp/RocketConsole/Dockerfile .

# Test image
podman run -it --rm -p 3501:3501/udp --name $imageServer "$acrName.azurecr.io/$imageServer"
podman run -it --rm --name $imageConsole "$acrName.azurecr.io/$imageConsole"

# Login to ACR
$accessToken = az acr login --name $acrName --expose-token --query accessToken -o tsv

# Build images in ACR
az acr build --registry $acrName --image "$imageServer" ./src/cpp/RocketServer
az acr build --registry $acrName --image "$imageConsole" ./src/cpp/RocketConsole

# Push images
podman login "$acrName.azurecr.io" -u 00000000-0000-0000-0000-000000000000 -p $accessToken
podman push "$acrName.azurecr.io/$imageServer"
podman push "$acrName.azurecr.io/$imageConsole"

# Deploy to ACI
$aci = az container create --resource-group $resourceGroup --name "$imageServer" --image "$acrName.azurecr.io/$imageServer" --cpu 1 --memory 1 --registry-login-server "$acrName.azurecr.io" --registry-username $acrName --registry-password $acrPassword --ports 3501 --protocol UDP --ip-address public --restart-policy Never --os-type linux -o json
$aci | jq -r .ipAddress.ip | clip

podman machine stop
```
