# Production Deployment Guide

This document describes how to deploy the Gomoku application to a Google Kubernetes Engine (GKE) cluster on Google Cloud Platform.

## Architecture

```
Internet
  |
  v
[K8s Ingress + TLS (cert-manager / letsencrypt)]
  |
  v
[gomoku-frontend (nginx + React SPA, 2 replicas)]
  |  serves static assets
  |  proxies /gomoku/* internally to:
  v
[envoy-gateway (Envoy proxy, 2 replicas)]
  |  LEAST_REQUEST load balancing
  |  circuit breakers, retries on 5xx
  |  health checks on /health
  v
[gomoku-workers (gomoku-httpd, HPA 2-20 replicas)]
  |  headless Service for pod discovery
  |  1 CPU per pod (single-threaded AI)
  |  topologySpreadConstraints for cross-zone distribution
```

All API traffic stays internal to the cluster. The frontend nginx proxies `/gomoku/*` requests to the Envoy gateway, which load-balances across gomoku-httpd worker pods. The Ingress controller handles TLS termination with certificates provisioned by cert-manager.

## Prerequisites

### Tools

- `gcloud` CLI ([install](https://cloud.google.com/sdk/docs/install))
- `kubectl` ([install](https://kubernetes.io/docs/tasks/tools/))
- `docker` ([install](https://docs.docker.com/get-docker/))

### GCP Setup

```bash
# Set your project
export PROJECT_ID="your-gcp-project-id"
export REGION="us-central1"
export CLUSTER_NAME="gomoku-cluster"

gcloud config set project $PROJECT_ID
gcloud config set compute/region $REGION
```

### Enable Required APIs

```bash
gcloud services enable \
  container.googleapis.com \
  artifactregistry.googleapis.com \
  compute.googleapis.com
```

## Step 1: Create GKE Cluster

```bash
gcloud container clusters create $CLUSTER_NAME \
  --region $REGION \
  --num-nodes 2 \
  --machine-type e2-standard-4 \
  --enable-autoscaling \
  --min-nodes 1 \
  --max-nodes 5 \
  --release-channel regular

# Get credentials for kubectl
gcloud container clusters get-credentials $CLUSTER_NAME --region $REGION
```

## Step 2: Create Artifact Registry Repository

```bash
export REGISTRY="${REGION}-docker.pkg.dev/${PROJECT_ID}/gomoku"

gcloud artifacts repositories create gomoku \
  --repository-format docker \
  --location $REGION

# Configure Docker authentication
gcloud auth configure-docker ${REGION}-docker.pkg.dev
```

## Step 3: Build and Push Docker Images

### Backend (gomoku-httpd)

```bash
docker build -t ${REGISTRY}/gomoku-httpd:latest .
docker push ${REGISTRY}/gomoku-httpd:latest
```

### Frontend (gomoku-frontend)

```bash
docker build -t ${REGISTRY}/gomoku-frontend:latest frontend/
docker push ${REGISTRY}/gomoku-frontend:latest
```

Or use the Makefile shortcut to build both:

```bash
make docker-build-all
```

Then tag and push:

```bash
docker tag gomoku-httpd:latest ${REGISTRY}/gomoku-httpd:latest
docker tag gomoku-frontend:latest ${REGISTRY}/gomoku-frontend:latest
docker push ${REGISTRY}/gomoku-httpd:latest
docker push ${REGISTRY}/gomoku-frontend:latest
```

## Step 4: Install nginx-ingress Controller

```bash
kubectl apply -f https://raw.githubusercontent.com/kubernetes/ingress-nginx/controller-v1.9.5/deploy/static/provider/cloud/deploy.yaml
```

Wait for the controller to get an external IP:

```bash
kubectl get svc -n ingress-nginx ingress-nginx-controller -w
```

Note the `EXTERNAL-IP` -- this is the IP your DNS must point to.

## Step 5: Install cert-manager for TLS

```bash
kubectl apply -f https://github.com/cert-manager/cert-manager/releases/download/v1.14.3/cert-manager.yaml

# Wait for cert-manager pods to be ready
kubectl wait --for=condition=ready pod -l app.kubernetes.io/instance=cert-manager -n cert-manager --timeout=120s
```

Create a ClusterIssuer for Let's Encrypt:

```bash
cat <<'EOF' | kubectl apply -f -
apiVersion: cert-manager.io/v1
kind: ClusterIssuer
metadata:
  name: letsencrypt-prod
spec:
  acme:
    server: https://acme-v02.api.letsencrypt.org/directory
    email: your-email@example.com
    privateKeySecretRef:
      name: letsencrypt-prod
    solvers:
      - http01:
          ingress:
            class: nginx
EOF
```

Replace `your-email@example.com` with your actual email address for certificate expiry notifications.

## Step 6: Configure DNS

Point your `gomoku.games` DNS A record to the external IP of the nginx-ingress controller (from Step 4). If you already have DNS configured, verify it resolves to the correct IP:

```bash
dig gomoku.games +short
```

## Step 7: Deploy to Kubernetes

Update the image references in kustomization to point to your Artifact Registry:

```bash
cd iac/k8s

# Update image references
kustomize edit set image \
  gomoku-httpd=${REGISTRY}/gomoku-httpd:latest \
  gomoku-frontend=${REGISTRY}/gomoku-frontend:latest
```

Deploy all resources:

```bash
kubectl apply -k iac/k8s/
```

Or from the project root:

```bash
make k8s-deploy
```

## Step 8: Verify Deployment

### Check Pod Status

```bash
kubectl get pods -n gomoku
```

Expected output shows frontend, envoy-gateway, and worker pods all `Running`:

```
NAME                               READY   STATUS    RESTARTS   AGE
envoy-gateway-xxxxx-xxxxx          1/1     Running   0          1m
envoy-gateway-xxxxx-xxxxx          1/1     Running   0          1m
gomoku-frontend-xxxxx-xxxxx        1/1     Running   0          1m
gomoku-frontend-xxxxx-xxxxx        1/1     Running   0          1m
gomoku-workers-xxxxx-xxxxx         1/1     Running   0          1m
gomoku-workers-xxxxx-xxxxx         1/1     Running   0          1m
gomoku-workers-xxxxx-xxxxx         1/1     Running   0          1m
gomoku-workers-xxxxx-xxxxx         1/1     Running   0          1m
```

### Check Services

```bash
kubectl get svc -n gomoku
```

### Check Ingress and TLS Certificate

```bash
kubectl get ingress -n gomoku
kubectl get certificate -n gomoku
```

The certificate may take a few minutes to be issued by Let's Encrypt.

### Test the Application

```bash
# Health check via Ingress
curl -s https://gomoku.games/nginx-health

# Test API endpoint (start a game with AI moving first)
curl -s https://gomoku.games/gomoku/play \
  -H 'Content-Type: application/json' \
  -d '{"board_state":[],"moves":[],"board_size":15,"search_depth":3}'
```

Open https://gomoku.games/ in a browser to verify the React frontend loads and games play to completion.

## Scaling

### Manual Scaling

```bash
# Scale workers
kubectl scale deployment gomoku-workers -n gomoku --replicas=8

# Scale frontend
kubectl scale deployment gomoku-frontend -n gomoku --replicas=3
```

### Autoscaling

The HorizontalPodAutoscaler automatically scales workers between 2 and 20 replicas based on 70% CPU utilization:

```bash
kubectl get hpa -n gomoku
```

To adjust autoscaling parameters:

```bash
kubectl edit hpa gomoku-workers -n gomoku
```

## Monitoring

### View Logs

```bash
# Worker logs
kubectl logs -n gomoku -l app.kubernetes.io/component=worker --tail=100

# Envoy gateway logs
kubectl logs -n gomoku -l app.kubernetes.io/component=envoy-gateway --tail=100

# Frontend logs
kubectl logs -n gomoku -l app.kubernetes.io/component=frontend --tail=100
```

### Envoy Admin Interface

Port-forward to access the Envoy admin dashboard:

```bash
kubectl port-forward -n gomoku svc/envoy-gateway 9901:9901
```

Then visit http://localhost:9901 to view cluster stats, upstream health, and connection pools.

### Resource Usage

```bash
kubectl top pods -n gomoku
kubectl top nodes
```

## Troubleshooting

### Pods Not Starting

```bash
kubectl describe pod <pod-name> -n gomoku
kubectl logs <pod-name> -n gomoku
```

### Certificate Not Issuing

```bash
kubectl describe certificate gomoku-tls -n gomoku
kubectl describe order -n gomoku
kubectl logs -n cert-manager -l app.kubernetes.io/name=cert-manager
```

### Envoy Not Discovering Workers

```bash
# Check headless service resolves to pod IPs
kubectl run -it --rm debug --image=busybox -n gomoku -- nslookup gomoku-workers.gomoku.svc.cluster.local

# Check Envoy cluster status
kubectl port-forward -n gomoku svc/envoy-gateway 9901:9901
curl http://localhost:9901/clusters
```

### 503 Errors Under Load

All workers may be busy computing AI moves. Check HPA status and consider increasing `maxReplicas`:

```bash
kubectl get hpa -n gomoku
kubectl describe hpa gomoku-workers -n gomoku
```

## Updating

### Rolling Update (Zero Downtime)

```bash
# Build and push new images
docker build -t ${REGISTRY}/gomoku-httpd:v1.2.0 .
docker push ${REGISTRY}/gomoku-httpd:v1.2.0

docker build -t ${REGISTRY}/gomoku-frontend:v1.2.0 frontend/
docker push ${REGISTRY}/gomoku-frontend:v1.2.0

# Update image tags
cd iac/k8s
kustomize edit set image \
  gomoku-httpd=${REGISTRY}/gomoku-httpd:v1.2.0 \
  gomoku-frontend=${REGISTRY}/gomoku-frontend:v1.2.0

kubectl apply -k .
```

### Teardown

```bash
make k8s-delete

# Or manually
kubectl delete -k iac/k8s/
```

To also delete the GKE cluster:

```bash
gcloud container clusters delete $CLUSTER_NAME --region $REGION
```
